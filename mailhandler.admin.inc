<?php

/**
 * Return a form for editing or creating an individual mailbox.
 * 
 * I blame this ugliness on lack of late static bindings in PHP < 5.3.
 * Could and should be refactored.
 * 
 */
function mailhandler_mailbox_form($form_state, $mailbox, $clone = FALSE) {
  module_load_include('inc', 'mailhandler', 'mailhandler.retrieve');
  $form = $subform = array();
  // Prepare default values
  $settings = $mailbox->settings;
  $base_defaults = $settings['MailhandlerMailbox'];
  $defaults = $settings[get_class($mailbox)];
  // Fetch basic mailhandler settings form
  $form['MailhandlerMailbox'] = array(
    '#type' => 'fieldset',
    '#title' => t('Basic mailbox settings'),
    '#tree' => TRUE,
  );
  $form += MailhandlerMailbox::baseForm($form_state, $base_defaults);
  foreach ($form as $key => $value) {
    if ($key != 'actions' && $key != 'MailhandlerMailbox') {
      $form['MailhandlerMailbox'][$key] = $value;
      unset($form[$key]);
    }
  }
  $mailbox_info = $mailbox->getInfo();
  // Pass in mailbox object to use in _validate
  $form['#mailbox'] = $mailbox;
  // Load in sub form based on mailbox type.
  $class = get_class($mailbox);
  $subform += $mailbox->form($form_state, $defaults);
  $subform[$class] = array(
    '#type' => 'fieldset',
    '#title' => t($mailbox_info['title']),
    '#tree' => TRUE,
  );
  foreach ($subform as $key => $values) {
    if ($key != $class) {
      $subform[$class][$key] = $values;
      unset($subform[$key]);
    }
  }
  $form += $subform;
  return $form;
}

/**
 * Verify that the Mailbox is valid, and save it to the database.
 */
function mailhandler_mailbox_form_validate($form, &$form_state) {
  // Validate the base form
  MailhandlerMailbox::baseFormValidate($form, $form_state);
  // Then, validate the subform
  $form['#mailbox']->validate($form, $form_state);
}

/**
 * Save a mailbox to the database - determine whether adding or editing by checking for a mailbox id (mid)
 */
function mailhandler_mailbox_form_submit($form, &$form_state) {
  // Prepare values for submission.  Check which modules 
  $settings = array();
  $settings['mail'] = $form_state['values']['MailhandlerMailbox']['mail'];
  $settings['mailbox_type'] = $form_state['values']['MailhandlerMailbox']['mailbox_type'];
  
  $plugins = mailhandler_get_plugins('mailhandler', 'mailbox_plugin');
  foreach ($plugins as $plugin) {
    if (array_key_exists($plugin['name'], $form_state['values'])) {
      $settings['settings'][$plugin['name']] = $form_state['values'][$plugin['name']];
    }
  }
  if ($form_state['values']['mailbox']) {
    drupal_write_record('mailhandler', $settings, 'mail');
    drupal_set_message(t('Mailbox %mailbox updated.', array('%mailbox' => $form_state['values']['mailhandler']['mail'])));
  }
  else {
    drupal_write_record('mailhandler', $settings);    
    drupal_set_message(t('Mailbox %mailbox added.', array('%mailbox' => $form_state['values']['mailhandler']['mail'])));
  }
  $form_state['redirect'] = 'admin/content/mailhandler';
  return;
}


function mailhandler_add_new_mailbox($form_state) {
  $form = array();
  // Load all plugins of mailbox_plugin type, provide as options
  $plugins = mailhandler_get_plugins('mailhandler', 'mailbox_plugin');
  $mailbox_types = array();
  foreach ($plugins as $plugin) {
    $object = FALSE;
    $object = mailhandler_plugin_load_class('mailhandler', $plugin['name'], 'mailbox_plugin', 'handler');
    $info = $object->getInfo();
    $mailbox_types[$plugin['name']] = $info['title'] . '<div class="description">' . $info['description'] . '</div>';
  }
  $form['mailbox_type'] = array(
    '#type' => 'radios',
    '#title' => t('Choose a mailbox type'),
    '#description' => '',
    '#options' => $mailbox_types,
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Continue'),
  );
  return $form;
}

function mailhandler_add_new_mailbox_submit($form, &$form_state) {
  $form_state['redirect'] = 'admin/content/mailhandler/add/' . $form_state['values']['mailbox_type'];
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
