<?php

// $Id$

class MailhandlerMailbox {
  
  public $settings = NULL;
  public $mail = NULL;
  public $mailbox_type = NULL;
  public $domain = NULL;
  public $port = NULL;
  public $extraimap = NULL;
  public $folder = NULL;
  public $pass = NULL;
  public $limit = NULL;
  public $mime = NULL;
  public $delete_after_read = NULL;
  public $encoding = NULL;
  public $fromheader = NULL;
  
  /**
   * Constructor
   */
  function __construct($mailbox = FALSE) {
    // TODO not sure how PHP 5.3 will treat get_class where I'm calling the 
    // class name below explicitly.
    
    // Fetch settings from database, propagate object.
    if ($mailbox) {
      $this->settings = $mailbox->settings;
      $this->mail = $mailbox->mail;
      $this->mailbox_type = $mailbox->mailbox_type;
      $this->domain = $mailbox->settings['MailhandlerMailbox']['domain'];
      $this->port = $mailbox->settings['MailhandlerMailbox']['port'];
      $this->extraimap = $mailbox->settings['MailhandlerMailbox']['extraimap'];
      $this->folder = $mailbox->settings['MailhandlerMailbox']['folder'];
      $this->pass = $mailbox->settings['MailhandlerMailbox']['pass'];
      $this->limit = $mailbox->settings['MailhandlerMailbox']['limit'];
      $this->mime = $mailbox->settings['MailhandlerMailbox']['mime'];
      $this->delete_after_read = $mailbox->settings['MailhandlerMailbox']['delete_after_read'];
      $this->encoding = $mailbox->settings['MailhandlerMailbox']['encoding'];
      $this->fromheader = $mailbox->settings['MailhandlerMailbox']['fromheader'];
    }
    else {
      $this->mailbox_type = get_class($this);
    }
  }
  
  private function setInfo() {
    return array(
      'title' => 'Basic mailbox',
      'description' => 'Base settings for mailbox.',
    );
  }
  
  public function getInfo() {
    return $this->setInfo();
  }
  
  public function getSettings($setting = '', $object = FALSE) {
    if (!$setting) {
      return $this->settings;
    }
    else {
      return $this->settings[get_class($object)][$setting];
    }
  }
  
  /**
   * Settings form
   */
  public function form(&$form_state, $defaults) {
    $form = array();
    $form['mailto'] = array(
      '#type' => 'textfield',
      '#title' => t('Second E-mail address'),
      '#default_value' => $defaults['mailto'],
      '#description' => t('Optional. The e-mail address to which modules should send generated content.'),
    );
    $form['folder'] = array(
      '#type' => 'textfield',
      '#title' => t('Folder'),
      '#default_value' => $defaults['folder'],
      '#description' => t('Optional. The folder where the mail is stored. If you want this mailbox to read from a local folder, give the full path. Leave domain, port, name, and pass empty below. Remember to set the folder to readable and writable by the webserver.'),
    );
    $form['imap'] = array(
      '#type' => 'select',
      '#title' => t('POP3 or IMAP Mailbox'),
      '#options' => array('POP3', 'IMAP'),
      '#default_value' => $defaults['imap'],
      '#description' => t('If you wish to retrieve mail from a POP3 or IMAP mailbox instead of a Folder, select POP3 or IMAP. Also, complete the Mailbox items below.'),
    );
    $form['domain'] = array(
      '#type' => 'textfield',
      '#title' => t('Mailbox domain'),
      '#default_value' => $defaults['domain'],
      '#description' => t('The domain of the server used to collect mail.'),
    );
    $form['port'] = array(
      '#type' => 'textfield',
      '#title' => t('Mailbox port'),
      '#size' => 5, '#maxlength' => 5,
      '#default_value' => $defaults['port'],
      '#description' => t('The port of the mailbox used to collect mail (usually 110 for POP3, 143 for IMAP).'),
    );
    $form['name'] = array(
      '#type' => 'textfield',
      '#title' => t('Mailbox username'),
      '#default_value' => $defaults['name'],
      '#description' => t('This username is used while logging into this mailbox during mail retrieval.'),
    );
    $form['pass'] = array(
      '#type' => 'textfield',
      '#title' => t('Mailbox password'),
      '#default_value' => $defaults['pass'],
      '#description' => t('The password corresponding to the username above. Consider using a non-vital password, since this field is stored without encryption in the database.'),
    );
    // Allow administrators to configure the mailbox with extra IMAP commands (notls, novalidate-cert etc.)
    $form['extraimap'] = array(
      '#type' => 'textfield',
      '#title' => t('Extra commands'),
      '#default_value' => $defaults['extraimap'],
      '#description' => t('Optional. In some circumstances you need to issue extra commands to connect to your mail server (e.g. "/notls", "/novalidate-cert" etc.). See documentation for <a href="http://php.net/imap_open">imap_open</a>. Begin the string with a "/", separating each subsequent command with another "/".'),
    );
    $form['limit'] = array(
      '#type' => 'textfield',
      '#title' => t('Maximum messages to retrieve'),
      '#default_value' => $defaults['limit'],
      '#description' => t('To prevent timeout errors from large mailboxes you can limit the maximum number of messages that will be retrieved during each cron run.  A value of zero means that no limit will be applied. Some trial and error may be needed to find the optimum setting.'),
    );
    $form['encoding'] = array(
      '#type' => 'textfield',
      '#title' => t('Default character encoding'),
      '#default_value' => $defaults['encoding'],
      '#description' => t('The default character encoding to use when an incoming message does not define an encoding.')
    );
    $form['mime'] = array(
      '#type' => 'select',
      '#title' => t('MIME preference'),
      '#options' => array(
        'TEXT/HTML,TEXT/PLAIN' => 'HTML',
        'TEXT/PLAIN,TEXT/HTML' => t('Plain text'),
    ),
      '#default_value' => $defaults['mime'],
      '#description' => t('When a user sends an e-mail containing both HTML and plain text parts, use this part as the node body.'),
    );
    $form['delete_after_read'] = array(
      '#type' => 'checkbox', 
      '#title' => t('Delete messages after they are processed?'), 
      '#default_value' => $defaults['delete_after_read'], 
      '#description' => t('Uncheck this box to leave read messages in the mailbox. They will not be processed again unless they become marked as unread.  If you selected "POP3" as your mailbox type, you must check this box.')
    );
    $form['fromheader'] = array(
      '#type' => 'textfield', 
      '#title' => t('From header'), 
      '#default_value' => $defaults['fromheader'],
      '#description' => t('Use this e-mail header to determine the author of the resulting node. Admins usually leave this field blank (thus using the <strong>From</strong> header), but <strong>Sender</strong> is also useful when working with listservs.')
    );
    return $form;
  }
  
  function validate($form, &$form_state) {
    // Check whether valid email address.
    if ($error = user_validate_mail($form_state['values']['mail'])) {
      form_set_error('mail', $error);
    }
    // Check whether email address already exists
    if ($form_state['op'] === 'add') {
      if ($result = db_result(db_query("SELECT 1 FROM {mailhandler} WHERE mail = '%s'", $form_state['values']['mail']))) {
        form_set_error('mail', t('Mailbox %mail already exists.  Please use a unique mailbox name.', array('%mail' => $form_state['values']['mail'])));
      }
    }
    // Check whether mailto is valid.
    if ($form_state['values']['mailto'] && ($error = user_validate_mail($form_state['values']['mailto']))) {
      form_set_error('mailto', $error);
    }
    
    // Test POP/IMAP settings, and store result
    $mailbox_appears_ok = TRUE;

    if ($form_state['values']['domain'] && $form_state['values']['port'] && !is_numeric($form_state['values']['port'])) { // assume external mailbox
      form_set_error('port', t('Mailbox port must be an integer.'));
      $mailbox_appears_ok = FALSE;
    }

    if (!$form_state['values']['domain'] && !$form_state['values']['port'] && $form_state['values']['folder']) { // assume local folder
      // check read and write permission
      if (!is_readable($form_state['values']['folder']) || !is_writable($form_state['values']['folder'])) {
        form_set_error('port', t('The local folder has to be readable and writable by owner of the webserver process, e.g. nobody.'));
        $mailbox_appears_ok = FALSE;
      }
    }
    // @TODO get this working again.
//    // Test POP/IMAP settings are valid in all cases, and test connection if requested
//    if ($mailbox_appears_ok && $form_state['clicked_button']['#value'] == t('Test connection')) {
//      // Call the test function
//      mailhandler_test_mailbox($form_state);
//      // We don't want to save, so we set an 'invisible error' that means the form is not submitted
//      form_set_error('mailhandler');
//    }

    // If POP3 mailbox is chosen, messages should be deleted after processing.  Do not set an actual error
    // because this is helpful for testing purposes.
    if ($form_state['values']['imap'] == 0 && $form_state['values']['delete_after_read'] == 0) {
      drupal_set_message(t('Unless you check off "Delete messages after they are processed" when using a POP3 mailbox, old emails will be re-imported each
      time the mailbox is processed.'), 'warning');
    }
    
  }
  
}