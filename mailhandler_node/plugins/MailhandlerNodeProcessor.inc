<?php

// Create or delete MAILHANDLER_NODE_BATCH_SIZE at a time.
define('MAILHANDLER_NODE_BATCH_SIZE', 50);

class MailhandlerNodeProcessor extends FeedsProcessor {

  public function process(FeedsImportBatch $batch, FeedsSource $source) {
    $processed = $comments = $new_nodes = $updated_nodes = 0;
    while ($item = $batch->shiftItem()) {
      $object = new stdClass();
      $object->type = $this->config['content_type'];
      $this->map($item, $object);
      $commandplugin = $item['mailbox']->getSettings('commandplugin', $item['mailbox']);
      if (is_array($commandplugin) && !empty($commandplugin)) {
        $this->commands($item, $object);
      }
      // Copy any name/value pairs from In-Reply-To or References e-mail headers to $node. Useful for maintaining threading info.
      if (!empty($item['header']->references)) {
        // we want the final element in references header, watching out for white space
        $threading = substr(strrchr($item['header']->references, '<'), 0);
      }
      else if (!empty($item['header']->in_reply_to)) {
        $threading = str_replace(strstr($item['header']->in_reply_to, '>'), '>', $item['header']->in_reply_to); // Some MUAs send more info in that header.
      }
      if (isset($threading) && $threading = rtrim(ltrim($threading, '<'), '>')) { //strip angle brackets
        if ($threading) $object->threading = $threading;
        parse_str($threading, $tmp);
        if ($tmp['host']) {
          $tmp['host'] = ltrim($tmp['host'], '@'); // strip unnecessary @ from 'host' element
        }
        foreach ($tmp as $key => $value) {
          $object->$key = $value;
        }
      }
      // Put mimeparts on object
      $object->mimeparts = $item['mimeparts'];
      // Authenticate the message.
      $this->authenticate($item, $object);
      // Submit the object.
      if ($object->type == 'comment') {
        if ($saved = $this->submitComment($object, $item['header'], $item['mailbox'], $item['origbody'])) {
          $comments++;
        }
        $type = 'comment';
      }
      else {
        if ($saved = $this->submitNode($object, $item['header'], $item['mailbox'], $item['origbody'])) {
          $saved->is_new ? $new_nodes++ : $updated_nodes++;
        }
        $type = 'node';
      }
      // Invoke hook_mailhandler_post_save for modules to operate on the newly created/edited node/comment.
      _mailhandler_hooks('mailhandler_post_save', array($saved, $type));

      $processed++;
      if ($processed >= variable_get('mailhandler_node_batch_size', MAILHANDLER_NODE_BATCH_SIZE)) {
        return (1.0 / ($batch->total + 1)) * ($comments + $new_nodes + $updated_nodes); // Add + 1 to make sure that result is not 1.0 = finished.
      }
    }
    // Set messages.
    if ($comments) {
      drupal_set_message(t('Created !number comments.', array('!number' => $comments)));
    }
    elseif ($updated_nodes) {
      drupal_set_message(t('Updated !number nodes.', array('!number' => $updated_nodes)));
    }
    elseif ($new_nodes) {
      drupal_set_message(t('Created !number nodes.', array('!number' => $new_nodes)));
    }
    else {
      drupal_set_message(t('No content created nor updated.'));
    }
    return FEEDS_BATCH_COMPLETE;
  }

  protected function submitNode($node, $header, $mailbox, $origbody) {
    global $user;
    // Handle revision information now that the final 'type' is known
    if ($node->nid) {
      $vid = db_result(db_query('SELECT n.vid FROM {node} n WHERE n.nid = %d', $node->nid));
      if ($vid) {
        $node->revision = $node->vid = $vid;
      }
    }
    // Prepare the object only after we know the final node type.
    static $included;
    if (!$included) {
      module_load_include('inc', 'node', 'node.pages');
      $included = TRUE;
    }
    _mailhandler_node_object_prepare($node);
    // Invoke hook_mailhandler
    $node = _mailhandler_hooks('mailhandler', array($node, $header, $mailbox));
    list($fromaddress, $fromname) = mailhandler_get_fromaddress($header, $mailbox);
    // Reset the static cache
    form_set_error(NULL, '', TRUE);
    node_validate($node);
    if (!$error_messages = form_set_error()) {
      // Prepare the node for save and allow modules make changes
      $node = node_submit($node);
      // Save the node
      if (!empty($node->nid)) {
        if (node_access('update', $node)) {
          node_save($node);
          watchdog('mailhandler', 'Updated %title by %from.', array('%title' => $node->title, '%from' => $fromaddress));
        }
        else {
          $error_text = t('The e-mail address !from may not update !type items.', array('!from' => $fromaddress, '!type' => $node->type));
          watchdog('mailhandler', 'Node submission failure: %from may not update %type items.', array('%from' => $fromaddress, '%type' => $node->type), WATCHDOG_WARNING);
        }
      }
      else {
        if (node_access('create', $node)) {
          node_save($node);
          watchdog('mailhandler', 'Added %title by %from.', array('%title' => $node->title, '%from' => $fromaddress));
        }
        else {
          $error_text = t('The e-mail address !from may not create !type items.', array('!from' => $fromaddress, '!type' => $node->type));
          watchdog('mailhandler', 'Node submission failure: %from may not create %type items.', array('%from' => $fromaddress, '%type' => $node->type), WATCHDOG_WARNING);
        }
      }
      // Return the node is successfully saved
      if (!isset($error_text)) {
        return $node;
      }
    }
    else {
      $error_text = t('Your submission is invalid:');
      watchdog('mailhandler', 'Node submission failure: validation error.', array(), WATCHDOG_WARNING);
    }
    // TODO: check that an email is actually sent if only node_validate fails.
    if (isset($error_text)) {
      if ($mailbox['replies']) {
        $params = array('body' => $origbody, 'error_messages' => $error_messages, 'error_text' => $error_text, 'from' => $fromaddress, 'header' => $header, 'node' => $node);
        drupal_mail('mailhandler', 'mailhandler_error_node', $fromaddress, user_preferred_language($user), $params);
      }
    }
    // return FALSE if the node was not successfully saved
    return FALSE;
  }

  protected function submitComment($comment, $header, $mailbox, $origbody) {
    global $user;
    // Invoke hook_mailhandler
    $comment = _mailhandler_hooks('mailhandler', array($comment, $header, $mailbox));
    if (!$comment->subject) {
      $comment->subject = $comment->title;
    }
    // 'comment' is the 'body' on comments.  Make it so.
    $comment->comment = $comment->body;
    // Only do this if status is set in the first place so that we fall back on permission system.
    // This depends on status being properly handled by command processor
    if (property_exists($comment, 'status')) {
      // In comment module, status of 1 means unpublished, status of 0 means published.
      $comment->status == 1 ? $comment->status = 0 : $comment->status = 1;
    }
    // We want the comment to have the email time, not the current time
    $comment->timestamp = $comment->created;
    // comment_save gets an array
    $edit = (array)$comment;

    // post the comment. if unable, send a failure email when so configured
    $cid = comment_save($edit);
    if (!$cid && $mailbox['replies']) {
      // $fromaddress really refers to the mail header which is authoritative for authentication
      list($fromaddress, $fromname) = mailhandler_get_fromaddress($header, $mailbox);
      $error_text = t('Sorry, your comment experienced an error and was not posted. Possible reasons are that you have insufficient permission to post comments or the node is no longer open for comments.');
      $params = array('body' => $origbody, 'error_messages' => array(), 'error_text' => $error_text, 'from' => $fromaddress, 'header' => $header, 'node' => $comment);
      drupal_mail('mailhandler', 'mailhandler_error_comment', $fromaddress, user_preferred_language($user), $params);
      watchdog('mailhandler', 'Comment submission failure: %subject.', array('%subject' => $edit['subject']), WATCHDOG_ERROR);
    }
    return $cid;
  }
  
  public function clear(FeedsBatch $batch, FeedsSource $source) {
    // TODO
    ;
  }
    
  /**
   * Override parent::map() to load all available add-on mappers.
   */
  protected function map($source_item, $target_node) {
    self::loadMappers();
    return parent::map($source_item, $target_node);
  }
  
  /**
   * Override setTargetElement to operate on a target item that is a node.
   */
  public function setTargetElement($target_node, $target_element, $value) {
//    if (in_array($target_element, array('status', 'type', 'size', 'origbody'))) {
//      $target_node->$target_element = $value;
//    }
  }
  
  /**
   * Return available mapping targets.
   *
   * Static cached, may be called multiple times in a page load.
   */
  public function getMappingTargets() {
    $targets = array(
      'title' => array(
        'name' => t('Title'),
        'description' => t('The title of the node.'),
        'callback' => '_mailhandler_node_processor_target_title',
      ),
      'body' => array(
        'name' => t('Body'),
        'description' => t('The body of the node. The teaser will be the same as the entire body.'),
        'callback' => '_mailhandler_node_processor_target_body',
      ),
      'created' => array(
        'name' => t('Published date'),
        'description' => t('The UNIX time when a node has been published.'),
        'callback' => '_mailhandler_node_processor_target_created',
      ),
      'teaser' => array(
        'name' => t('Teaser'),
        'description' => t('The node teaser'),
        'callback' => '_mailhandler_node_processor_target_teaser',
      ),
      'threading' => array(
        'name' => t('Threading'),
        'description' => t('Email threading information'),
        'callback' => '_mailhandler_node_processor_target_threading',
      ),
    );
    // Let other modules expose mapping targets.
    self::loadMappers();
    // TODO: Does checking content type make sense here if commands can alter the content type?
    // Also, this is using node_processor... not specific to mailhandler.
    drupal_alter('feeds_node_processor_targets', $targets, $this->config['content_type']);

    return $targets;
  }

  /**
   * Loads on-behalf implementations from mappers/
   */
  protected static function loadMappers() {
    static $loaded = FALSE;
    if (!$loaded) {
      $path = drupal_get_path('module', 'feeds') .'/mappers';
      $files = drupal_system_listing('.*\.inc$', $path, 'name', 0);
      foreach ($files as $file) {
        if (strstr($file->filename, '/mappers/')) {
          require_once("./$file->filename");
        }
      }
      // Rebuild cache.
      module_implements('', FALSE, TRUE);
    }
    $loaded = TRUE;
  }
  
  /*
   * Parse and apply commands.
   */
  public function commands($item, &$object) {
    if (($plugins = $item['mailbox']->getSettings('commandplugin', $item['mailbox'])) && is_array($plugins) && !empty($plugins)) {
      foreach ($plugins as $plugin) {
        if ($class = mailhandler_plugin_load_class('mailhandler_node', $plugin, 'commands_plugin', 'handler')) {
          $class->parse($item, &$object);
          $class->process($item, &$object); 
        }
      }
    }
  }
  
  /*
   * Authenticate the message.
   */
  public function authenticate($item, &$object) {
    // TODO: allow for multiple authenticate plugins to be weighted and implemented.
    if (($plugin = $item['mailbox']->getSettings('authenticateplugin', $item['mailbox']))) {
      if ($class = mailhandler_plugin_load_class('mailhandler_node', $plugin, 'authenticate_plugin', 'handler')) {
        $class->authenticate($item, &$object);
      }
    }
  }
  
  /**
   * Override parent::configForm().
   */
  public function configForm(&$form_state) {
    $types = node_get_types('names');
    $form = array();
    $form['content_type'] = array(
      '#type' => 'select',
      '#title' => t('Content type'),
      '#description' => t('Choose node type to create from this mailbox. <strong>Note:</strong> Users with "import !feed_id feeds" permissions will be able to <strong>import</strong> nodes of the content type selected here regardless of the node level permissions. Further, users with "clear !feed_id permissions" will be able to <strong>delete</strong> imported nodes regardless of their node level permissions.', array('!feed_id' => $this->id)),
      '#options' => $types,
      '#default_value' => $this->config['content_type'],
    );
    return $form;
  }
  
  /**
   * Override parent::configDefaults().
   */
  public function configDefaults() {
    $types = node_get_types('names');
    $type = isset($types['story']) ? 'story' : key($types);
    return array(
      'content_type' => $type,
      'mappings' => array(),
    );
  }

}

/*
 * Private mapping callback functions
 */

function _mailhandler_node_processor_target_body($object, $target, $value) {
  $lines = explode("\n", $value);
  for ($i = 0; $i < count($lines); $i++) {
    $line = trim($lines[$i]);
    $words = explode(' ', $line);
    // Look for a command line. if not present, note which line number is the boundary
    if (substr($words[0], -1) == ':' && !isset($endcommands)) {
      continue;
    }
    else {
      if (!isset($endcommands)) $endcommands = $i;
    }
  }
  $tmp = array_slice($lines, $endcommands, $i - $endcommands);
  $body = implode("\n", $tmp);
  // TODO: handle stripping sig, except we don't know the sig delimiter in this scope.
  // Idea to let user define what their sig separater is and use that based on who
  // the mail is from.  Again, don't have needed info in this scope.
  // To deal w/ scope issues, could stuff something onto $node->mailhandler.
  $object->$target = $body;
}

function _mailhandler_node_processor_target_title($object, $target, $value) {
  $subjectarr = imap_mime_header_decode($value);
  if (empty($subjectarr)) {
    $object->title = truncate_utf8(trim(decode_entities(strip_tags(check_markup($object->body)))), 29, TRUE);
  }
  else {
    for ($i = 0; $i < count($subjectarr); $i++) {
      if ($subjectarr[$i]->charset != 'default') {
        $object->title = drupal_convert_to_utf8($subjectarr[$i]->text, $subjectarr[$i]->charset);
      }
      else {
        $object->title = $subjectarr[$i]->text;
      }
    }
  }
}

function _mailhandler_node_processor_target_created($object, $target, $value) {
  $date = strtotime($value);
  $object->$target = $date;
  $object->changed = $date;
}

function _mailhandler_node_processor_target_teaser($object, $target, $value) {
  $object->$target = node_teaser($object->body);
}

/**
 * Prepare the node
 * 
 * Basically a copy of node_object_prepare but does not set the user nor
 * overwrite values if already set.
 * 
 * @param object $node
 */
function _mailhandler_node_object_prepare(&$node) {
  // Set up default values, if required.
  $node_options = variable_get('node_options_'. $node->type, array('status', 'promote'));
  // If this is a new node, fill in the default values.
  if (!isset($node->nid)) {
    foreach (array('status', 'promote', 'sticky') as $key) {
      if (!isset($node->$key)) {
        $node->$key = in_array($key, $node_options);
      }
    }
  }
  else {
    $node->date = format_date($node->created, 'custom', 'Y-m-d H:i:s O');
    // Remove the log message from the original node object.
    $node->log = NULL;
  }
  // Always use the default revision setting.
  $node->revision = in_array('revision', $node_options);

  node_invoke($node, 'prepare');
  node_invoke_nodeapi($node, 'prepare');
}

/**
 * 
 * @param $hook
 *   The hook name to call.
 * @param $args
 *   Arguments to pass to the hook.
 */
function _mailhandler_hooks($hook, $args = array()) {
  foreach (module_list() as $name) {
    if (module_hook($name, $hook)) {
      // Nothing is passed by reference, so this works.
      if (!$object = call_user_func_array($name . '_' . $hook, $args)) {
        // Exit if a module has handled the submitted data.
        break;
      }
    }
  }
  return $object;
}