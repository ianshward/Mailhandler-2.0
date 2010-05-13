<?php

class MailhandlerMailbox {
  var $mailbox;
  var $mailto;
  var $imap;
  var $domain;
  var $port;
  var $name;
  var $pass;
  var $extraimap;
  var $mime;
  var $security;
  var $replies;
  var $fromheader;
  var $commands;
  var $sigseparator;
  var $delete_after_read;
  var $enabled;
  var $format;
  var $authentication;

  /**
   * Constructor
   */
  function __construct() {
    $this->imap = 0;
    $this->mime = 'TEXT/HTML,TEXT/PLAIN';
    $this->security = 0;
    $this->replies = 1;
    $this->delete_after_read = 1;
    $this->enabled = 1;
    $this->filter = FILTER_FORMAT_DEFAULT;
  }

  function load() {
    // load from database?
    // already use on mailhandler_mailboxes_load
  }

  function save() {
    // update or insert into database.
  }


}
