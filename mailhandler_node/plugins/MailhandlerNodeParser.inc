<?php
// $Id: FeedsCSVParser.inc,v 1.8 2010/03/29 04:02:37 alexb Exp $

/**
 * Parses a given file as a CSV file.
 */
class MailhandlerNodeParser extends FeedsParser {

  /**
   * Implementation of FeedsParser::parse().
   */
  public function parse(FeedsImportBatch $batch, FeedsSource $source) {
    $fetched = $batch->getRaw();
    if ($fetched['new']) {
      module_load_include('inc', 'mailhandler', 'mailhandler.retrieve');
      $messages = array();
      $messages = mailhandler_retrieve($fetched['mailbox'], $fetched['mailbox']['limit'], $fetched['mailbox']['encoding']);
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
    // Custom cases.  TODO: There's probably a smarter way to do this.  The reason I
    // do this is because it seems like setTargetElement expects these property names
    // to match up with actual property names on the node.
    $message['title'] = $message['subject'];
    $message['body'] = $message['origbody'];
    $message['created'] = $message['date'];
    $message['threading'] = array('references' => $message['header']->references, 'in_reply_to' => $message['header']->in_reply_to); //$message;
  }
  /*
   * This defines sources which user's can select to map values to.
   */
  public function getMappingSources() {
    $sources = array(
      'title' => array(
        'name' => t('Subject'),
        'description' => t('The email subject.'),
      ),
      'body' => array(
        'name' => t('Body'),
        'description' => t('The cleaned email body, commands removed.'),
      ),
      'created' => array(
        'name' => t('Date'),
        'description' => t('The date of the email')
      ),
      'type' => array(
        'name' => t('Type'),
        'description' => t('The type of node'),
      ),
      'origbody' => array(
        'name' => t('Original body'),
        'description' => t('The original body of the email'),
      ),
      'mimeparts' => array(
        'name' => t('Mimeparts'),
        'description' => t('The email mimeparts'),
      ),
      'threading' => array(
        'name' => t('Threading'),
        'description' => t('Email threading information'),
      ),
    );
    // Make all IMAP header keys available as selectable mapping sources.
    $parts = array('date', 'subject', 'message_id', 'toaddress', 'to', 'fromaddress', 'from', 'reply_toaddress', 'reply_to', 'senderaddress', 'sender', 'Recent', 'Unseen', 'Flagged', 'Answered', 'Deleted', 'Draft', 'Msgno', 'MailDate', 'Size', 'udate');
    foreach ($parts as $part) {
      $sources[$part] = array(
        'title' => t($part),
        'description' => t('IMAP header property.'),
      );
    }
    drupal_alter('feeds_node_processor_sources', $sources);
    return $sources;
  }

}
