<?php

class MailhandlerCommandsDefault {
   protected $commands = NULL;
  
  /**
   * Parse the commands
   */
   function parse($item, &$node) {
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
     if (!empty($this->commands)) {
       // The node type must be set first in order to determine possible vocabs.
       foreach ($this->commands as $command) {
         if ($command[0] == 'type') {
           $node->type = $command[1];
         }
       }
       $vocabs = taxonomy_get_vocabularies($node->type);
       $node->taxonomy = array();
       foreach ($this->commands as $data) {
         // TODO: allow for nested arrays in commands ... Possibly trim() values after explode().
         // If needed, turn this command value into an array
         if (substr($data[1], 0, 1) == '[' && substr($data[1], -1, 1) == ']') {
           $data[1] = rtrim(ltrim($data[1], '['), ']'); //strip brackets
           $data[1] = explode(",", $data[1]);
           // allow for key value pairs
           foreach ($data[1] as $key => $value) {
             $data_tmp = explode(":", $value, 2);
             if (isset($data_tmp[1])) { // is it a key value pair?
               // remove old, add as key value pair
               unset($data[1][$key]);
               $data_tmp[0] = trim($data_tmp[0]);
               $data[1][$data_tmp[0]] = $data_tmp[1];
             }
           }
         }
         $data[0] = strtolower(str_replace(' ', '_', $data[0]));
         // if needed, map term names into IDs. this should move to taxonomy_mailhandler()
         if ($data[0] == 'taxonomy' && !is_numeric($data[1][0])) {
           array_walk($data[1], 'mailhandler_term_map');
           // Only add term if node type can use that term's vocab
           $term = taxonomy_get_term($data[1][0]);
           if (array_key_exists($term->vid, $vocabs)) {
             $node->taxonomy = array_merge($node->taxonomy, $data[1]);
           }
           unset($data[0]);
         }
         else if (substr($data[0], 0, 9) == 'taxonomy[' && substr($data[0], -1, 1) == ']'){
           // make sure a valid vid is passed in:
           $vid = substr($data[0], 9, -1);
           $vocabulary = taxonomy_vocabulary_load($vid);
           // if the vocabulary is not activated for that node type, unset $data[0], so the command will be ommited from $node
           // TODO: add an error message
           if (!in_array($node->type, $vocabulary->nodes)) {
             unset($data[0]);
           }
           else if (!$vocabulary->tags) {
             array_walk($data[1], 'mailhandler_term_map', $vid);
             $node->taxonomy = array_merge($node->taxonomy, $data[1]);
             unset($data[0]);
           }
           else if ($vocabulary->tags) {
             // for freetagging vocabularies, we just pass the list of terms
             $node->taxonomy['tags'][$vid] = implode(',', $data[1]);
             unset($data[0]); // unset, so it won't be included when populating the node object
           }
         }
         if (!empty($data[0])) {
           $node->$data[0] = $data[1];
         }
         // 'status' for nodes and comments are backwards.  This makes the permission system handle it.
         if ($node->type == 'comment') {
           unset($node->status);
         }
       }
     }
   }

}
