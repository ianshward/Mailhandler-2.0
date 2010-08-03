<?php

abstract class MailhandlerNodeAuthenticate {
  
  /**
   * Authenticates an incoming message.
   * 
   * @param $item
   *   Array containing message headers, body, and mailbox information.
   * @param $node
   *   A node object.  May 
   */
  abstract public function authenticate($item, &$object);
  
}