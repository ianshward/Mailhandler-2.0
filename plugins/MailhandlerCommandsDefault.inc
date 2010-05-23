<?php

class MailhandlerCommandsDefault {
   protected $commands = NULL;
  
  /**
   * Parse the commands
   */
   function parse($item, &$node) {
     watchdog('commands', 'parsing commands');
     $body = $item['body'];
     // Prepend the default commands.  User-added commands will override the default commands.
     if ($item['mailbox']['commands']) {
       $body = trim($item['mailbox']['commands']) ."\n". $body;
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
   * Parse and process commands
   */
   function process($item, &$node) {
     watchdog('commands', 'processing commands');
     // TODO: need to handle the special case of making 'status' set correctly for comments
     // Important: should be unset in case of type = comment so that commands can then explicitly set
     // the status in case of comments.
     //     if ($node->type == 'comment') {
     //       // 'status' for nodes and comments are backwards.  This makes the permission system handle it.
     //       unset($node->status);
     //     }
     }
  

}
