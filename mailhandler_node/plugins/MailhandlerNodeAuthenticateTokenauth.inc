<?php

/*
 *   Authenticate message based on token from tokenauth module
 *   If the user's token is found somewhere in the "to" field, assign that user's uid and name 
 *   to the node object.  A rough search for the token somewhere in the "toaddress" is performed
 *   instead of an exact, ordered match in order to allow some freedom in the format of allowed
 *   "toaddress".  For example, if using a catchall email address, the toaddress could be:
 *   
 *   f93ksj35dx@example.com - where f93ksj35dx is the user's token
 *   or alternatively:
 *   f93ksj35dx-foo@example.com - where f93ksj35dx is the user's token and foo is the name of an
 *   Organic Group to which the message should be assigned.  
 *   
 *   A rough search allows for different approaches to use this single authentication method.
 *   
 */
class MailhandlerAuthenticateTokenauth extends MailhandlerAuthenticate {
  
  public function authenticate($item, &$node) {
    if (module_exists('tokenauth')) {
      list($fromaddress, $fromname) = mailhandler_get_fromaddress($item['header'], $item['mailbox']);
      // If user with given email address exists and their token is in the toaddress, allow.
      if (($from_user = mailhandler_user_load($fromaddress, $node->pass, $item['mailbox']))
      && strpos($header->to[0]->mailbox, tokenauth_get_token($from_user->uid)) !== FALSE) {
        $node->uid = $from_user->uid;
        $node->name = $from_user->name;
      }
      // Try using mailalias email aliases
      else if (function_exists('mailalias_user') && ($from_user = mailhandler_user_load_alias($fromaddress, $node, $item['mailbox']'))
      && strpos($header->to[0]->mailbox, tokenauth_get_token($from_user->uid)) !== FALSE) {
        $node->uid = $from_user->uid;
        $node->name = $from_user->name;
      }
      else {
        // If token authentication fails, try as anonymous.
        $node->uid = 0;
        $node->name = $fromname;
      }
    }
  }
}