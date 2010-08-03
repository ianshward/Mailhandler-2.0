<?php
  
class MailhandlerAuthenticateDefault extends MailhandlerAuthenticate {
  
  public function authenticate($item, &$object) {
    list($fromaddress, $fromname) = mailhandler_get_fromaddress($item['header'], $item['mailbox']);
    if ($from_user = mailhandler_user_load($fromaddress, $object->pass, $mailbox)) {
      $object->uid = $from_user->uid;
      $object->name = $from_user->name;
    }
    // Try using mailalias email aliases
    else if (function_exists('mailalias_user') && $from_user = mailhandler_user_load_alias($fromaddress, $object, $mailbox)) {
      $object->uid = $from_user->uid;
      $object->name = $from_user->name;
    }
    else {
      // Authentication failed.  Try as anonymous.
      $object->uid = 0;
      $object->name = $fromname;
    }
  }

}
