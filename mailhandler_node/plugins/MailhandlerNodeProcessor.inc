<?php

// Create or delete MAILHANDLER_NODE_BATCH_SIZE at a time.
define('MAILHANDLER_NODE_BATCH_SIZE', 50);

class MailhandlerNodeProcessor extends FeedsProcessor {

  public function process(FeedsImportBatch $batch, FeedsSource $source) {
    $processed = $comments = $new_nodes = $updated_nodes = 0;
    while ($item = $batch->shiftItem()) {
      $node = new stdClass();
      $node->type = $this->config['content_type'];
      $this->map($item, $node);
      $commandplugin = $item['mailbox']->getSettings('commandplugin', $item['mailbox']);
      if (is_array($commandplugin) && !empty($commandplugin)) {
        $this->commands($item, $node);
      }
      // Handle revision information now that the final 'type' is known
      if ($node->nid && $node->type != 'comment') {
        $vid = db_result(db_query('SELECT n.vid FROM {node} n WHERE n.nid = %d', $node->nid));
        if ($vid) {
          $node->revision = $node->vid = $vid;
        }
      }
      $this->authenticate($item, $node);
      // Prepare the object only after we know the final node type.
      static $included;
      if (!$included) {
        module_load_include('inc', 'node', 'node.pages');
        $included = TRUE;
      }
      _mailhandler_node_object_prepare($node);
      // Modules may override node elements before submitting. they do so by returning the node.
      foreach (module_list() as $name) {
        if (module_hook($name, 'mailhandler')) {
          $function = $name .'_mailhandler';
          if (!($node = $function($node, $item['header'], $item['mailbox']))) {
            // Exit if a module has handled the submitted data.
            break;
          }
        }
      }
      if ($node->type == 'comment') {
        if ($object = $this->submitComment($node, $item['header'], $item['mailbox'], $item['origbody'])) {
          $comments++;
        }
        $type = 'comment';
      }
      else {
        if ($object = $this->submitNode($node, $item['header'], $item['mailbox'], $item['origbody'])) {
          $node->is_new ? $new_nodes++ : $updated_nodes++;
        }
        $type = 'node';
      }
      // Invoke a second hook for modules to operate on the newly created/edited node/comment.
      foreach (module_list() as $name) {
        if (module_hook($name, 'mailhandler_post_save')) {
          $function = $name .'_mailhandler_post_save';
          // Pass in the result from submit (which could be a cid, or a node object)
          $function($object, $type);
        }
      }
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

  protected function submitComment($node, $header, $mailbox, $origbody) {
    global $user;
    if (!$node->subject) {
      $node->subject = $node->title;
    }
    // 'comment' is the 'body' on comments.  Make it so.
    $node->comment = $node->body;
    // Only do this if status is set in the first place so that we fall back on permission system.
    // This depends on status being properly handled by command processor
    if (property_exists($node, 'status')) {
      // In comment module, status of 1 means unpublished, status of 0 means published.
      $node->status == 1 ? $node->status = 0 : $node->status = 1;
    }
    // We want the comment to have the email time, not the current time
    $node->timestamp = $node->created;
    // comment_save gets an array
    $edit = (array)$node;

    // post the comment. if unable, send a failure email when so configured
    $cid = comment_save($edit);
    if (!$cid && $mailbox['replies']) {
      // $fromaddress really refers to the mail header which is authoritative for authentication
      list($fromaddress, $fromname) = mailhandler_get_fromaddress($header, $mailbox);
      $error_text = t('Sorry, your comment experienced an error and was not posted. Possible reasons are that you have insufficient permission to post comments or the node is no longer open for comments.');
      $params = array('body' => $origbody, 'error_messages' => array(), 'error_text' => $error_text, 'from' => $fromaddress, 'header' => $header, 'node' => $node);
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
  public function commands($item, &$node) {
    if (($plugins = $item['mailbox']->getSettings('commandplugin', $item['mailbox'])) && is_array($plugins) && !empty($plugins)) {
      foreach ($plugins as $plugin) {
        if ($class = mailhandler_plugin_load_class('mailhandler_node', $plugin, 'commands', 'handler')) {
          $class->parse($item, &$node);
          $class->process($item, &$node); 
        }
      }
    }
  }
  
  /*
   * Authenticate the message.
   */
  public function authenticate($item, &$node) {
    // TODO: allow for multiple authenticate plugins to be weighted and implemented.
    if (($plugin = $item['mailbox']->getSettings('authenticateplugin', $item['mailbox']))) {
      if ($class = mailhandler_plugin_load_class('mailhandler_node', $plugin, 'authenticate', 'handler')) {
        $class->authenticate($item, &$node);
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

function _mailhandler_node_processor_target_body($node, $target, $value) {
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
  $node->$target = $body;
}

function _mailhandler_node_processor_target_title($node, $target, $value) {
  $subjectarr = imap_mime_header_decode($value);
  if (empty($subjectarr)) {
    $node->title = truncate_utf8(trim(decode_entities(strip_tags(check_markup($node->body)))), 29, TRUE);
  }
  else {
    for ($i = 0; $i < count($subjectarr); $i++) {
      if ($subjectarr[$i]->charset != 'default') {
        $node->title = drupal_convert_to_utf8($subjectarr[$i]->text, $subjectarr[$i]->charset);
      }
      else {
        $node->title = $subjectarr[$i]->text;
      }
    }
  }
}

function _mailhandler_node_processor_target_created($node, $target, $value) {
  $date = strtotime($value);
  $node->$target = $date;
  $node->changed = $date;
}

function _mailhandler_node_processor_target_teaser($node, $target, $value) {
  $node->$target = node_teaser($node->body);
}

// @TODO This should probably just be done in process() and not as a target callback
function _mailhandler_node_processor_target_threading($node, $target, $value) {
  //$message['threading'] = array('references' => $message['header']->references, 'in_reply_to' => $message['header']->in_reply_to); //$message;
  // This message['threading'] is what i had in the parser
  
  // Copy any name/value pairs from In-Reply-To or References e-mail headers to $node. Useful for maintaining threading info.
  if (!empty($value['references'])) {
    // we want the final element in references header, watching out for white space
    $threading = substr(strrchr($value['references'], '<'), 0);
  }
  else if (!empty($value['in_reply_to'])) {
    $threading = str_replace(strstr($value['in_reply_to'], '>'), '>', $value['in_reply_to']); // Some MUAs send more info in that header.
  }
  if (isset($threading) && $threading = rtrim(ltrim($threading, '<'), '>')) { //strip angle brackets
    if ($threading) {
      $node->threading = $threading;
    }
    parse_str($threading, $tmp);
    if ($tmp['host']) {
      $tmp['host'] = ltrim($tmp['host'], '@'); // strip unnecessary @ from 'host' element
    }
    foreach ($tmp as $key => $val) {
      $node->$key = $val;
    }
  }
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

