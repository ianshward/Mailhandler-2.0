<?php

class MailhandlerNodeCommandsDefault extends MailhandlerNodeCommands {
  //protected $commands = NULL;

  /**
   * Parse and process commands
   */
  function process($item, &$object) {
    if (!empty($this->commands)) {
      // The node type must be set first in order to determine possible vocabs.
      foreach ($this->commands as $command) {
        if ($command[0] == 'type') {
          $object->type = $command[1];
        }
      }
      if ($object->type != 'comment') {
        $vocabs = taxonomy_get_vocabularies($object->type);
        $object->taxonomy = array();
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
              $object->taxonomy = array_merge($object->taxonomy, $data[1]);
            }
            unset($data[0]);
          }
          else if (substr($data[0], 0, 9) == 'taxonomy[' && substr($data[0], -1, 1) == ']') {
            // make sure a valid vid is passed in:
            $vid = substr($data[0], 9, -1);
            $vocabulary = taxonomy_vocabulary_load($vid);
            // if the vocabulary is not activated for that node type, unset $data[0], so the command will be ommited from $object
            // TODO: add an error message
            if (!in_array($object->type, $vocabulary->nodes)) {
              unset($data[0]);
            }
            else if (!$vocabulary->tags) {
              array_walk($data[1], 'mailhandler_term_map', $vid);
              $object->taxonomy = array_merge($object->taxonomy, $data[1]);
              unset($data[0]);
            }
            else if ($vocabulary->tags) {
              // for freetagging vocabularies, we just pass the list of terms
              $object->taxonomy['tags'][$vid] = implode(',', $data[1]);
              unset($data[0]); // unset, so it won't be included when populating the node object
            }
          }
          if (!empty($data[0])) {
            $object->$data[0] = $data[1];
          }
        }
      }
      else {
        // 'status' for nodes and comments are backwards.  This makes the permission system handle it.
        unset($object->status);
      }
    }
  }
}
