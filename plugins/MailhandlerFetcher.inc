<?php
// $Id$

/**
 * @file
 * Checks for new messages on an IMAP stream
 */

/**
 * Definition of the import batch object created on the fetching stage by
 * FeedsFileFetcher.
 */
class MailhandlerImportBatch extends FeedsImportBatch {
  protected $mailbox;

  /**
   * Constructor.
   */
  public function __construct($mailbox) {
    $this->mailbox = $mailbox;
    parent::__construct();
  }

  /**
   * Implementation of FeedsImportBatch::getRaw();
   */
  public function getRaw() {
    $mailbox = mailhandler_mailboxes_load($this->mailbox);
    module_load_include('inc', 'mailhandler', 'mailhandler.retrieve');
    if ($result = mailhandler_open_mailbox($mailbox)) {
      $new = mailhandler_get_unread_messages($result);
      // Makes more intuitive.
      empty($new) ? $new = FALSE : $new = TRUE;
      return array('new' => $new, 'mailbox' => $mailbox); 
    }
    else {
      drupal_set_message('Unable to connect to mailbox.');
    }
  }
}

/**
 * Fetches data via HTTP.
 */
class MailhandlerFetcher extends FeedsFetcher {

  /**
   * Implementation of FeedsFetcher::fetch().
   */
  public function fetch(FeedsSource $source) {
    $source_config = $source->getConfigFor($this);
    return new MailhandlerImportBatch($source_config['mailbox']);
  }

  /**
   * Source form.
   */
  public function sourceForm($source_config) {
    $options = array();
    $mailboxes = mailhandler_mailboxes_load();
    foreach ($mailboxes as $key => $mailbox) {
      $options[$key] = $key;
    }
    $form = array();
    // Load all the available mailboxes.
    $form['mailbox'] = array(
      '#type' => 'select',
      '#title' => t('Mailbox'),
      '#description' => t('Select a mailbox to use'),
      '#default_value' => isset($source_config['mailbox']) ? $source_config['mailbox'] : '',
      '#options' => $options,
    );
    return $form;
  }

}
