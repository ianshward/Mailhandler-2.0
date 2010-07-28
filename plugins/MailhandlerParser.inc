<?php
// $Id$

/**
 * Parses an IMAP stream
 */
class MailhandlerParser extends FeedsParser {

  /**
   * Implementation of FeedsParser::parse().
   */
  public function parse(FeedsImportBatch $batch, FeedsSource $source) {
    $fetched = $batch->getRaw();
    if ($fetched['new']) {
      module_load_include('inc', 'mailhandler', 'mailhandler.retrieve');
      $messages = array();
      $messages = mailhandler_retrieve($fetched['mailbox'], $fetched['mailbox']->limit, $fetched['mailbox']->encoding);
      // Populate batch.
      if (!empty($messages)) {
        foreach ($messages as &$message) {
          $this->parseExtensions($message);
        }
        $batch->setItems($messages);
      }
    }
    else {
      if (isset($fetched['new'])) {
        drupal_set_message('No new messages.');
      }
    }
  }

  /*
   * Set known sources and parse additional sources from body.
   */
  public function parseExtensions(&$message) {
    // Populate $message with all values from 'header' object.
    $parts = (array) $message['header'];
    foreach ($parts as $key => $value) {
      // Some keys are already taken, so do not overwrite them.
      if (!in_array($key, array('header', 'origbody', 'mimeparts', 'mailbox'))) {
        $message[$key] = $value;
      }
    }
  }
  /*
   * This defines sources which user's can select to map values to.
   */
  public function getMappingSources() {
    $sources = array();
    // Make all IMAP header keys available as selectable mapping sources.
    $parts = array('date', 'subject', 'message_id', 'toaddress', 'to',
      'fromaddress', 'from', 'reply_toaddress', 'reply_to', 'senderaddress', 
      'sender', 'Recent', 'Unseen', 'Flagged', 'Answered', 'Deleted', 'Draft', 
      'Msgno', 'MailDate', 'Size', 'udate',
      'origbody', 'mimeparts',
    );
    foreach ($parts as $part) {
      $sources[$part] = array(
        'title' => t($part),
        'description' => t('IMAP header property.'),
      );
    }
    // @TODO this alter may need a better name.
    drupal_alter('mailhandler_parser_sources', $sources);
    return $sources;
  }

}
