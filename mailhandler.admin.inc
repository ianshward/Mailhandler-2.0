<?php

/**
 * Return a form for editing or creating an individual mailbox.
 * 
 * This design was somewhat influenced by the lack of late static bindings in
 * PHP < 5.3
 */
function mailhandler_mailbox_form($form_state, $op, $mail, $type = 'MailhandlerMailbox') {
  module_load_include('inc', 'mailhandler', 'mailhandler.retrieve');
  $form = array(
    '#theme' => 'mailhandler_mailbox_form',
    '#mailhandler_op' => $op,
    '#mailhandler_mail' => $mail,
    '#mailhandler_class' => $type,
  );
  if ($op == 'edit') {
    // Load the object and get its type
    $mailbox = mailhandler_mailboxes_load($mail);
    $form['#mailhandler_mailbox'] = $mailbox;
    $defaults = $mailbox->getDefaults();
    $defaults = $defaults[$type];
  }
  // Make sure the class is loaded, then call its form.
  if ($class = mailhandler_plugin_load_class('mailhandler', $type, 'mailbox_plugin', 'handler')) {
    $form += call_user_func_array(array($class, 'form'), array($form_state, $defaults));
  }
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Submit'),
  );
  // TODO: else, fail gracefully.
  return $form;
}

/**
 * Verify that the Mailbox is valid, and save it to the database.
 */
function mailhandler_mailbox_form_validate($form, &$form_state) {
  if ($class = mailhandler_plugin_load_class('mailhandler', $form['#mailhandler_class'], 'mailbox_plugin', 'handler')) {
    call_user_func_array(array($class, 'validate'), array($form, &$form_state));
  }
}

/**
 * Save a mailbox to the database - determine whether adding or editing by checking for a mailbox id (mid)
 */
function mailhandler_mailbox_form_submit($form, &$form_state) {
  // Prepare values for submission.  Check which modules 
  $record = array();
  $record['mail'] = $form['#mailhandler_mail'];
  $record['mailbox_type'] = $form['#mailhandler_class'];
  // Only affect the settings key which pertains to the mailbox Type being edited.
  if ($form['#mailhandler_mailbox'] && $current = db_result(db_query("SELECT settings FROM {mailhandler} WHERE mail = '%s'", $record['mail']))) {
    $current = unserialize($current);
    $current[$form['#mailhandler_class']] = $form_state['values'];
    $record['settings'] = $current;
  }
  else {
    $record['settings'][$form['#mailhandler_class']] = $form_state['values'];
  }
  if ($form['#mailhandler_mailbox']) {
    drupal_write_record('mailhandler', $record, 'mail');
    drupal_set_message(t('Mailbox %mailbox updated.', array('%mailbox' => $form['#mailhandler_mail'])));
  }
  else {
    drupal_write_record('mailhandler', $record);    
    drupal_set_message(t('Mailbox %mailbox added.', array('%mailbox' => $form['#mailhandler_mail'])));
  }
  $form_state['redirect'] = 'admin/content/mailhandler';
  return;
}


function mailhandler_add_new_mailbox($form_state) {
  $form = array();
  $form['mail'] = array(
    '#type' => 'textfield',
    '#title' => t('Email address'),
    '#description' => t('Enter email address of mailbox'),
    '#required' => TRUE,
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Continue'),
  );
  return $form;
}

function mailhandler_add_new_mailbox_validate($form, &$form_state) {
  if ($form_state['values']['mail'] && ($error = user_validate_mail($form_state['values']['mail']))) {
    form_set_error('mail', $error);
  }
  if ($form_state['values']['mail'] && mailhandler_mailboxes_load($form_state['values']['mail'])) {
    form_set_error('mail', t('The email address %address is already in use', array('%address' => $form_state['values']['mail'])));
  }
}

function mailhandler_add_new_mailbox_submit($form, &$form_state) {
  $form_state['redirect'] = 'admin/content/mailhandler/add/' . $form_state['values']['mail'];
}

/**
 *  Submit handler for mailbox delete button
 *  Redirect to delete handler if delete is clicked
 */
function mailhandler_delete_mailbox_submit($form, &$form_state) {
  $form_state['redirect'] = array('admin/content/mailhandler/delete/'.$form_state['values']['mid']);
}

/**
 * Confirm/Delete Mailbox
 */
function mailhandler_admin_delete_confirm($form_state, $mid) {
//TODO: this is broken
  $info = db_fetch_object(db_query("SELECT mid, mail FROM {mailhandler} WHERE mid = %d", $mid));

  $form['mid'] = array(
    '#type' => 'hidden',
    '#value' => $mid,
  );

  return confirm_form(
      $form,
      t('Do you wish to delete mailbox %mailbox?', array('%mailbox' => $info->mail)),
      'admin/content/mailhandler',
      t('This action cannot be undone.'),
      t('Delete'),
      t('Cancel')
    );
}

// TODO: this is broken
function mailhandler_admin_delete_confirm_submit($form, &$form_state) {
  $info = db_fetch_object(db_query("SELECT mid, mail FROM {mailhandler} WHERE mid = %d", $form_state['values']['mid']));
  db_query("DELETE FROM {mailhandler} WHERE mid = %d", $form_state['values']['mid']);
  watchdog('mailhandler', 'Mailhandler: Mailbox %mailbox deleted', array('%mailbox' => $info->mail), WATCHDOG_INFO);
  drupal_set_message(t('Mailbox %mailbox deleted', array('%mailbox' => $info->mail)));
  $form_state['redirect'] = 'admin/content/mailhandler';
  return;
}


/**
 * Test connection to a mailbox, and output results as a message
 */
// TODO: try form_altering this in or re-integrate it.
function mailhandler_test_mailbox(&$form_state) {
  module_load_include('inc', 'mailhandler', 'mailhandler.retrieve');
  $mailbox = $form_state['values'];
  $result = mailhandler_open_mailbox($mailbox);

  if ($result) {
    drupal_set_message('Mailhandler was able to connect to the mailbox.');
    imap_close($result);
  }
  else {
    if ($mailbox['domain']) {
      form_set_error('mailhandler', t('%c connection to %m failed.', array('%c' => ($mailbox['imap'] ? 'IMAP' : 'POP3'), '%m' => $mailbox['mail'])));
    }
    else {
      form_set_error('mailhandler', t('Mailhandler could not access local folder: %m', array('%m' => $mailbox['mail'])));
    }
  }

  return;
}

/**
 * Theme function for theme('mailhandler_mailbox_form')
 */
function theme_mailhandler_mailbox_form($form) {
  $output = '<div class="mailhandler-settings clear-block">';
  $output .= '<div class="left-bar">';
  $plugins = mailhandler_get_plugins('mailhandler', 'mailbox_plugin');
  foreach ($plugins as $plugin) {
    $object = FALSE;
    $object = mailhandler_plugin_load_class('mailhandler', $plugin['name'], 'mailbox_plugin', 'handler');
    $info = $object->getInfo();
    $output .= l(t($info['title']), 'admin/content/mailhandler/'.$form['#mailhandler_op'].'/'.$form['#mailhandler_mail'].'/'.$plugin['name']);
  }
  $output .= '</div>';
  $output .= '<div class="main-bar">';
  $output .= drupal_render($form);
  $output .= '</div';
  $output .= '</div>';
  return $output;
}
