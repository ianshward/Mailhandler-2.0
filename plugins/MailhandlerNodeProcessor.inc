<?php

class MailhandlerNodeProcessor extends FeedsProcessor {

  public function process(FeedsImportBatch $batch, FeedsSource $source) {
    while ($item = $batch->shiftItem()) {
      $node = new stdClass();
      $node->type = $this->config['content_type'];
      $this->map($item, $node);
      if ($item['mailbox']['commandplugin']) {
        $this->commands($item, $node);
      }
      $this->authenticate($item, $node);
      // Prepare the object only after we know the final node type.
      static $included;
      if (!$included) {
        module_load_include('inc', 'node', 'node.pages');
        $included = TRUE;
      }
      node_object_prepare($node);
      
      // Submit
      if ($node->type == 'comment') {
        $nid = $this->submitComment($node, $item['header'], $item['mailbox'], $item['origbody']);
        $type = 'comment';
      }
      else {
        $nid = $this->submitNode($node, $item['header'], $item['mailbox'], $item['origbody']);
        $type = 'node';
      }
      
    }
    // TODO: review this.  Handle counting.
    return FEEDS_BATCH_COMPLETE;
  }
  
  protected function MessagePrepare($header, $body, $mailbox) {
    // Initialise a node object
    $node = new stdClass();
    $node->pass = NULL;

    // Copy any name/value pairs from In-Reply-To or References e-mail headers to $node. Useful for maintaining threading info.
    if (!empty($header->references)) {
      // we want the final element in references header, watching out for white space
      $threading = substr(strrchr($header->references, '<'), 0);
    }
    else if (!empty($header->in_reply_to)) {
      $threading = str_replace(strstr($header->in_reply_to, '>'), '>', $header->in_reply_to); // Some MUAs send more info in that header.
    }
    if (isset($threading) && $threading = rtrim(ltrim($threading, '<'), '>')) { //strip angle brackets
      if ($threading) $node->threading = $threading;
      parse_str($threading, $tmp);
      if ($tmp['host']) {
        $tmp['host'] = ltrim($tmp['host'], '@'); // strip unnecessary @ from 'host' element
      }
      foreach ($tmp as $key => $value) {
        $node->$key = $value;
      }
    }
    // Apply defaults to the $node object, and allow modules to add default values
    require_once($base_path . 'modules/node/node.pages.inc');
    node_object_prepare($node);
    // In order to fall back to the permission system for comment status, the status property must
    // be unset if type is comment.  It will get set by explicit commands, and if not, by
    // comment_save itself.
    if($node->type == 'comment') {
      unset($node->status);
    }
    
    // This is raw, w/ the sig, the commands, etc. 
    $node->body = $body;

    if (empty($node->teaser)) $node->teaser = node_teaser($node->body);

    // decode encoded subject line
    $subjectarr = imap_mime_header_decode($header->subject);
    if (empty($subjectarr)) {
      $node->title = truncate_utf8(trim(decode_entities(strip_tags(check_markup($node->body)))), 29, TRUE);
    }
    else {
      for ($i = 0; $i < count($subjectarr); $i++) {
        if ($subjectarr[$i]->charset != 'default')
        $node->title .= drupal_convert_to_utf8($subjectarr[$i]->text, $subjectarr[$i]->charset);
        else
        $node->title .= $subjectarr[$i]->text;
      }
    }
    $node->date = $node->changed = format_date($header->udate, 'custom', 'Y-m-d H:i:s O');
    $node->format = $mailbox['format'];

    // If an nid command was supplied, and type is not 'comment', append the revision number
    if ($node->nid && $node->type != 'comment') {
      $vid = db_result(db_query('SELECT n.vid FROM {node} n WHERE n.nid = %d', $node->nid));
      if ($vid) {
        $node->revision = $node->vid = $vid;
      }
    }

    return $node;
  }

  protected function MessageProcess($header, $origbody, $mailbox) {
    // TODO: this does not necessarily need to happen here... could be done right before submit?
    mailhandler_switch_user();
    // We must process before authenticating because the password may be in Commands
    $node = $this->MessagePrepare($header, $origbody, $mailbox);

    // Authenticate the message
    // TODO: ctools plugin
    if (!$node = mailhandler_mailhandler_authenticate('execute', $mailbox['authentication'], array($node, $header, $origbody, $mailbox))) {
      watchdog('mailhandler', 'Message failed authentication', array(), WATCHDOG_ERROR);
      return FALSE;
    }

    // Put $mimeparts on the node
    $node->mimeparts = $mimeparts;

    // We need to change the current user this has to be done here to allow modules to create users
    mailhandler_switch_user($node->uid);

    // Modules may override node elements before submitting. they do so by returning the node.
    foreach (module_list() as $name) {
      if (module_hook($name, 'mailhandler')) {
        $function = $name .'_mailhandler';
        if (!($node = $function($node, $result, $i, $header, $mailbox))) {
          // Exit if a module has handled the submitted data.
          break;
        }
      }
    }

    if ($node) {
      if ($node->type == 'comment') {
        $nid = $this->MessageCommentSubmit($node, $header, $mailbox, $origbody);
        $type = 'comment';
      }
      else {
        $nid = $this->MessageNodeSubmit($node, $header, $mailbox, $origbody);
        $type = 'node';
      }
    }

    // Invoke a second hook for modules to operate on the newly created/edited node/comment.
    foreach (module_list() as $name) {
      if (module_hook($name, 'mailhandler_post_save')) {
        $function = $name .'_mailhandler_post_save';
        // Pass in the $nid (which could be a $cid, depending on $node->type)
        $function($nid, $type);
      }
    }

    // switch back to original user
    mailhandler_switch_user();
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
    if (!$node->subject) $node->subject = $node->title;
    // When submitting comments, 'comment' means actualy the comment's body, and not the comments status for a node.
    // We need to reset the comment's body, so it doesn't colide with a default 'comment' command.
    $node->comment = $node->body;
    // comment_save will not fall back to permission system if we set the status explicitly
    // See comment_save.  += will not overwrite an existing array property.
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
    if ($target_element == 'body') {
      $target_node->teaser = $value;
      $target_node->body = $value;
    }
    elseif (in_array($target_element, array('title', 'status', 'created', 'type', 'size', 'origbody'))) {
      $target_node->$target_element = $value;
    }
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
    if (($plugins = $item['mailbox']['commandplugin']) && !empty($plugins)) {
      foreach ($plugins as $plugin) {
        if ($class = mailhandler_plugin_load_class($plugin, 'commands', 'handler')) {
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
    if (($plugin = $item['mailbox']['authenticateplugin'])) {
      if ($class = mailhandler_plugin_load_class($plugin, 'authenticate', 'handler')) {
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
  // TODO: strip the commands and signature, and what's left is the body.
  // But, where should commands actually be run?
  $node->$target = $value;
  
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



