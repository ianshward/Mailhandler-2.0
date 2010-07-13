<?php

class MailhandlerNodeMailbox extends MailhandlerMailbox {
  
//  function __construct() {
//    parent::__construct();
//  }
  
  private function setInfo() {
    return array(
      'title' => 'Mailhandler Node Mailbox',
      'description' => 'Create nodes with new messages',
    );
  }
  
  public function getInfo() {
    return $this->setInfo();
  }
  
  public function form(&$form_state, $defaults) {
    $form = array();
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
    $form['security'] = array(
      '#type' => 'radios',
      '#title' => t('Security'),
      '#options' => array(t('Disabled'), t('Require password')),
      '#default_value' => isset($defaults['security']) ? $defaults['security'] : 0,
      '#description' => t('Disable security if your site does not require a password in the Commands section of incoming e-mails. Note: Security=Enabled and MIME preference=HTML is an unsupported combination.')
    );
    $form['replies'] = array(
      '#type' => 'radios', 
      '#title' => t('Send error replies'), 
      '#options' => array(t('Disabled'), t('Enabled')), 
      '#default_value' => $defaults['replies'],
      '#description' => t('Send helpful replies to all unsuccessful e-mail submissions. Consider disabling when a listserv posts to this mailbox.')
    );
    $form['fromheader'] = array(
      '#type' => 'textfield', 
      '#title' => t('From header'), 
      '#default_value' => $defaults['fromheader'],
      '#description' => t('Use this e-mail header to determine the author of the resulting node. Admins usually leave this field blank (thus using the <strong>From</strong> header), but <strong>Sender</strong> is also useful when working with listservs.')
    );
    $form['sigseparator'] = array(
      '#type' => 'textfield', 
      '#title' => t('Signature separator'), 
      '#default_value' => $defaults['sigseparator'], 
      '#description' => t('All text after this string will be discarded. A typical value is <strong>"-- "</strong> that is two dashes followed by a blank in an otherwise empty line. Leave blank to include signature text in nodes.')
    );
    $form['delete_after_read'] = array(
      '#type' => 'checkbox', 
      '#title' => t('Delete messages after they are processed?'), 
      '#default_value' => $defaults['delete_after_read'], 
      '#description' => t('Uncheck this box to leave read messages in the mailbox. They will not be processed again unless they become marked as unread.  If you selected "POP3" as your mailbox type, you must check this box.')
    );
    $form['commands'] = array(
      '#type' => 'textarea',
      '#title' => t('Default commands'),
      '#description' => t('A set of commands which are added to each message.'),
      '#default_value' => $defaults['commands'],
    );
    $form['encoding'] = array(
      '#type' => 'textfield',
      '#title' => t('Default character encoding'),
      '#default_value' => $defaults['encoding'],
      '#description' => t('The default character encoding to use when an incoming message does not define an encoding.')
    );
    $form['max'] = array(
      '#type' => 'textfield',
      '#title' => t('Maximum messages to retrieve'),
      '#default_value' => $defaults['limit'],
      '#description' => t('To prevent timeout errors from large mailboxes you can limit the maximum number of messages that will be retrieved during each cron run.  A value of zero means that no limit will be applied. Some trial and error may be needed to find the optimum setting.'),
    );
    ctools_include('plugins');
    $options = array();
    $plugins = mailhandler_get_plugins('mailhandler_node', 'commands_plugin');
    foreach ($plugins as $key => $info) {
      $options[$key] = $info['name'];
    }
    $form['commandplugin'] = array(
      '#type' => 'select',
      '#title' => t('Command plugins'),
      '#description' => t('Choose the command plugins to use'),
      '#options' => $options,
      '#multiple' => TRUE,
      '#default_value' => $defaults['commandplugin'],
    );
    $options = array(); //$options[0] = t('None');
    $plugins = mailhandler_get_plugins('mailhandler_node', 'authenticate_plugin');
    foreach ($plugins as $key => $info) {
      $options[$key] = $info['name'];
    }
    $form['authenticateplugin'] = array(
      '#type' => 'select',
      '#title' => t('Authentication plugin'),
      '#description' => t('Choose an authentication plugin'),
      '#options' => $options,
      '#default_value' => $defaults['authenticateplugin'],
      '#required' => FALSE,
    );
    // Allow administrators to select the format of saved nodes/comments
    $form['format'] = filter_form($defaults['format']);

    return $form;
  }
  
  function validate($form, &$form_state) {
    
  }

}