<?php

// $Id$

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