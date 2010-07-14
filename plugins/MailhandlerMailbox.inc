<?php

class MailhandlerMailbox {
  
  public $settings = NULL;
  public $mail = NULL;
  public $mailbox_type = NULL;
  
  /**
   * Constructor
   */
  function __construct($mailbox = FALSE) {
    // Fetch settings from database, propagate object.
    if ($mailbox) {
      $this->settings = $mailbox->settings;
      $this->mail = $mailbox->mail;
      $this->mailbox_type = $mailbox->mailbox_type;
    }
    else {
      $this->mailbox_type = get_class($this);
    }
  }
  
  private function setInfo() {
    return array(
      'title' => 'Simple mailhandler mailbox',
      'description' => 'This mailbox type does not do much, yet',
    );
  }
  
  public function getInfo() {
    return $this->setInfo();
  }
  
  public function getDefaults() {
    return $this->settings;
  }
  
  /**
   * Settings form
   */
  public function form(&$form_state, $defaults) {
    $form = array();
    $form['mail'] = array(
      '#type' => 'textfield',
      '#title' => t('E-mail address'),
      '#default_value' => $defaults['mail'] ? $defaults['mail'] : arg(4),
      '#description' => t('The e-mail address to which users should send their submissions.'),
      '#required' => TRUE,
    );
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

    return $form;
  }
  
  function validate($form, &$form_state) {
    
  }
  
  /**
   * Validate form submission
   */
  function baseFormValidate($form, &$form_state) {

//    // Check for valid email address
//    if ($error = user_validate_mail($form_state['values']['MailhandlerMailbox']['mail'])) {
//      form_set_error('mail', $error);
//    }
//    
//    krumo($form_state);
//
//    if ($form_state['values']['mailto'] && ($error = user_validate_mail($form_state['values']['mailto']))) {
//      form_set_error('mailto', $error);
//    }
//
//    // Test POP/IMAP settings, and store result
//    $mailbox_appears_ok = TRUE;
//
//    if ($form_state['values']['domain'] && $form_state['values']['port'] && !is_numeric($form_state['values']['port'])) { // assume external mailbox
//      form_set_error('port', t('Mailbox port must be an integer.'));
//      $mailbox_appears_ok = FALSE;
//    }
//
//    if (!$form_state['values']['domain'] && !$form_state['values']['port'] && $form_state['values']['folder']) { // assume local folder
//      // check read and write permission
//      if (!is_readable($form_state['values']['folder']) || !is_writable($form_state['values']['folder'])) {
//        form_set_error('port', t('The local folder has to be readable and writable by owner of the webserver process, e.g. nobody.'));
//        $mailbox_appears_ok = FALSE;
//      }
//    }
//
//    // Test POP/IMAP settings are valid in all cases, and test connection if requested
//    if ($mailbox_appears_ok && $form_state['clicked_button']['#value'] == t('Test connection')) {
//      // Call the test function
//      mailhandler_test_mailbox($form_state);
//
//      // We don't want to save, so we set an 'invisible error' that means the form is not submitted
//      form_set_error('mailhandler');
//    }
//
//    // If POP3 mailbox is chosen, messages must be deleted after processing.
//    if ($form_state['values']['imap'] == 0 && $form_state['values']['delete_after_read'] == 0) {
//      form_set_error('delete_after_read', t('You must check off "Delete messages after they are processed" when using a POP3 mailbox.'));
//    }

  }

}