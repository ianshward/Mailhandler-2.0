<?php
  
class MailhandlerAuthenticateDefault {
  
  public function authenticate($item, &$node) {
    list($fromaddress, $fromname) = mailhandler_get_fromaddress($item['header'], $item['mailbox']);
    if ($from_user = mailhandler_user_load($fromaddress, $node->pass, $mailbox)) {
      $node->uid = $from_user->uid;
      $node->name = $from_user->name;
    }
    // Try using mailalias email aliases
    else if (function_exists('mailalias_user') && $from_user = mailhandler_user_load_alias($fromaddress, $node, $mailbox)) {
      $node->uid = $from_user->uid;
      $node->name = $from_user->name;
    }
    else {
      // Authentication failed.  Try as anonymous.
      $node->uid = 0;
      $node->name = $fromname;
    }
  }

}
