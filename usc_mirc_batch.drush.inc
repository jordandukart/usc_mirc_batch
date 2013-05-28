<?php

/**
 * @file
 * A drush script for updating MIRC@SC's  content models.
 */

/**
 * Implements hook_drush_command().
 */
function usc_mirc_batch_drush_command() {
  $items = array();
  $items['mirc_update_objects'] = array(
    'callback' => 'usc_mirc_batch_update_objects',
    'description' => 'Updates all objects for a given content model.',
    'arguments' => array(
      'fedora_cmodel' => 'The name of the old content model.',
    ),
    'options' => array(),
    'examples' => array(
      'simple example' => 'drush -u 1 -l http://example.com usc_mirc_update_objects namespace',
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_LOGIN,
  );
  $items['mirc_generate_pbcore'] = array(
    'callback' => 'usc_mirc_generate_pb',
    'description' => 'Generates missing PBCORE for Access Variants.',
    'arguments' => array(),
    'options' => array(),
    'examples' => array(
      'simple example' => 'drush -u 1 -l http://example.com mirc_generate_pbcore',
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_LOGIN,
  );
  $items['mirc_add_handles'] = array(
    'callback' => 'usc_mirc_generate_handles',
    'description' => 'Creates and appends Handles for existing mezzanineCModel objects',
    'arguments' => array(),
    'options' => array(),
    'examples' => array(
      'simple example' => 'drush -u 1 -l http://example.com mirc_add_handles',
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_LOGIN,
  );
  return $items;
}

/**
 * Kicks off the type of update based on old content model.
 *
 * @param string $fedora_cmodel
 *   The old Fedora content model.
 */
function usc_mirc_batch_update_objects($fedora_cmodel) {
  $connection = islandora_get_tuque_connection();
  switch ($fedora_cmodel) {
    case 'usc:test-vro':
      usc_mirc_batch_update_vros($connection);
      break;

    case 'usc:test-mezzanine':
      usc_mirc_batch_update_mezzanines($connection);
      break;

    case 'usc:test-access':
      usc_mirc_batch_update_access($connection);
      break;

    default:
      drush_set_error("Invalid CModel passed", "$fedora_cmodel is not a valid CModel to update", "Error: ");
      break;
  }
}

/**
 * Batch updates the old preservation masters.
 *
 * @param Tuque $connection
 *   A Tuque connection.
 */
function usc_mirc_batch_update_vros($connection) {
  $query = 'select $object from <#ri> where
  $object <fedora-model:hasModel> $model
  and $model <mulgara:is> <info:fedora/usc:test-vro>';
  $results = $connection->repository->ri->itqlQuery($query);
  foreach ($results as $result) {
    usc_mirc_batch_update_object($result['object']['value'], 'usc:vroCModel');
  }
}

/**
 * Batch updates the old mezzanines.
 *
 * @param Tuque $connection
 *   A Tuque connection.
 */
function usc_mirc_batch_update_mezzanines($connection) {
  $query = 'select $object from <#ri> where
  $object <fedora-model:hasModel> $model
  and $model <mulgara:is> <info:fedora/usc:test-mezzanine>';
  $results = $connection->repository->ri->itqlQuery($query);
  foreach ($results as $result) {
    usc_mirc_batch_update_object($result['object']['value'], 'usc:mezzanineCModel');
  }
}

/**
 * Batch updates the old access variants.
 *
 * @param Tuque $connection
 *   A Tuque connection.
 */
function usc_mirc_batch_update_access($connection) {
  // XXX: Most of the objects in prod have bad relationships for "hasModel"...
  $query = 'select $object from <#ri> where
  (
    $object <fedora-model:hasModel> $model or
    $object <fedora-rels-ext:hasModel> $model
  )
  and $model <mulgara:is> <info:fedora/usc:test-access>';
  $results = $connection->repository->ri->itqlQuery($query);
  foreach ($results as $result) {
    usc_mirc_batch_update_access_copy($result['object']['value']);
  }
}

/**
 * Update access copy...
 *
 * Removes a bad relationship (if it exists).
 *
 * @param string $pid
 *   The ID of the given access copy.
 */
function usc_mirc_batch_update_access_copy($pid) {
  usc_mirc_batch_update_object($pid, 'usc:accessCModel');

  $object = islandora_object_load($pid);

  // XXX: Most of the access copies in prod have bad relationships...
  // let's nuke 'em.
  $bad_preds = $object->relationships->remove(FEDORA_RELS_EXT_URI, 'hasModel');
}

/**
 * Updates an old object's content model.
 *
 * @param string $pid
 *   Pid of the object we are updating.
 * @param string $cmodel
 *   The new content model for the object.
 */
function usc_mirc_batch_update_object($pid, $cmodel) {
  $object = islandora_object_load($pid);
  unset($object->models);
  $object->models = array(
    '0' => $cmodel,
    '1' => 'fedora-system:FedoraObject-3.0',
  );
  watchdog('MIRC@SC update', 'Updated the cmodel for @pid', array('@pid' => $pid), WATCHDOG_INFO);
}

/**
 * Queries to find all access variants.
 */
function usc_mirc_generate_pb() {
  $connection = islandora_get_tuque_connection();
  $query = 'select $object from <#ri> where
  $object <fedora-model:hasModel> $model
  and $model <mulgara:is> <info:fedora/usc:accessCModel>';
  $results = $connection->repository->ri->itqlQuery($query);
  foreach ($results as $result) {
    usc_mirc_generate_pbcore($result['object']['value']);
  }
}

/**
 * Generates, adds PBCore and updates the DC datastream of an object.
 *
 * @param string $pid
 *   The pid of the object we are updating.
 */
function usc_mirc_generate_pbcore($pid) {
  module_load_include('inc', 'usc_mirc', 'includes/utilities');
  $object = islandora_object_load($pid);

  if (!$object['PBCORE']) {
    $video_url = $object['VIDEO']->url;
    $streaming_url = variable_get('usc_mirc_streaming_url', 'http://dvr-streaming.mirc.sc.edu');
    if (strpos($video_url, $streaming_url) === 0) {
      $local_path = format_string('!local_access/!file_part', array(
        '!local_access' => variable_get('usc_mirc_streaming_folder', '/mnt/mirc/mezz/access'),
        '!file_part' => drupal_substr($video_url, drupal_strlen($streaming_url) + 1),
      ));
    }
    if (!empty($local_path)) {
      $parent_pid = $object->relationships->get(FEDORA_RELS_EXT_URI, 'isDerivativeOf');
      $parent_object = islandora_object_load($parent_pid[0]['object']['value']);
      if (isset($parent_object['PBCORE'])) {
        $params = usc_mirc_retrieve_pbcore_fields($parent_object);
        usc_mirc_generate_video_metadata($local_path, $object, $params);
        drush_log('success', "Added PBCORE and updated DC metadata to $object->id");
      }
      else {
        drush_log('error', "The mezzanine ($parent_object->id) contains no PBCORE while attempting to add to $object->id");
      }
    }
    else {
      drush_log('error', "Unable to determine the path to the local file system for $object->id");
    }
  }
}

/**
 * Queries to find all Mezzanines and determines which do not have Handle values.
 */
function usc_mirc_generate_handles() {
  $connection = islandora_get_tuque_connection();
  $query = 'select $object from <#ri> where
  $object <fedora-model:hasModel> $model
  and $model <mulgara:is> <info:fedora/usc:mezzanineCModel>';
  $results = $connection->repository->ri->itqlQuery($query);
  foreach ($results as $result) {
    usc_mirc_generate_handle($result['object']['value']);
  }
}

/**
 * Constructs and appends Handles to existing Mezzanines.
 *
 * @param string $pid
 *   The pid of the Mezzanine object we are updating.
 */
function usc_mirc_generate_handle($pid) {
  module_load_include('inc', 'islandora_handle', 'includes/handle');
  $object = islandora_object_load($pid);
  $response = islandora_handle_construct_handle($object);
  if ($response->code == 201) {
    islandora_handle_append_handle($object, 'PBCORE', drupal_get_path('module', 'usc_mirc') . '/transforms/handle_the_PBCore.xsl');
    islandora_handle_add_to_dc_datastream($object);
    drush_log('success', "Created Handle and attempted to append to PBCORE for $object->id. Check watchdogs for any potential errors.");
  }
  else {
    drush_log('error', "Unable to create Handle for $object->id");
  }
}
