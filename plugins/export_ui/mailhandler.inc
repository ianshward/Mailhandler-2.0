<?php

$plugin = array(
  'schema' => 'mailhandler',
  'access' => 'administer mailhandler',
  'menu' => array(
    'menu prefix' => 'admin/content',
    'menu item' => 'mailhandler',
    'menu title' => 'Mailhandler Mailboxes', 
    'menu description' => 'Administer site mailhandler objects.',
  ),
  'title singular' => t('mailhandler mailbox'),
  'title singular proper' => t('Mailhandler Mailbox'),
  'title plural' => t('mailhandler mailboxes'),
  'title plural proper' => t('Mailhandler Mailboxes'),
  'form' => array(
      'settings' => 'mailhandler_mailbox_form',
      'validate' => 'mailhandler_mailbox_form_validate',
      'submit' => 'mailhandler_mailbox_form_submit',
  ),
);