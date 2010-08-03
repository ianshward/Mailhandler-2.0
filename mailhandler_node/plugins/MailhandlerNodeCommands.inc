<?php

abstract class MailhandlerNodeCommands {
  
  protected $commands = NULL;
  
  /**
   * Parse commands from email body
   * 
   * @param $item
   *   Array which contains mailbox, message header, and body information.
   * @param $object
   *   Node object of the node being built for submission.
   */
  public function parse($item, &$object) {
    $body = $item['body'];
    // Prepend the default commands.  User-added commands will override the default commands.
    if ($item['mailbox']->commands) {
      $body = trim($item['mailbox']->commands) ."\n". $body;
    }
    $commands = array(); $endcommands = NULL;
    // Collect the commands and locate signature
    $lines = explode("\n", $body);
    for ($i = 0; $i < count($lines); $i++) {
      $line = trim($lines[$i]);
      $words = explode(' ', $line);
      // Look for a command line. if not present, note which line number is the boundary
      if (substr($words[0], -1) == ':' && !isset($endcommands)) {
        // Looks like a name: value pair
        $commands[$i] = explode(': ', $line, 2);
      }
      else {
        if (!isset($endcommands)) $endcommands = $i;
      }
    }
    $this->commands = $commands;
  }
  
  /**
   * Process parsed commands by applying / manipulating the node object.
   * 
   * @param $item
   *   Array which contains mailbox, message header, and body information.
   * @param $object
   *   Node object of the node being built for submission.
   */
  abstract public function process($item, &$object);
  
}