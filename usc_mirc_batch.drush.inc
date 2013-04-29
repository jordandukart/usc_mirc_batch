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
  return $items;
}

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

function usc_mirc_batch_update_vros($connection) {
  $query = 'select $object from <#ri> where
  $object <fedora-model:hasModel> $model
  and $model <mulgara:is> <info:fedora/usc:test-vro>';
  $results = $connection->repository->ri->itqlQuery($query);
  foreach ($results as $result) {
    usc_mirc_batch_update_object($result['object']['value'], 'usc:vroCModel');
  }
}

function usc_mirc_batch_update_mezzanines($connection) {
  $query = 'select $object from <#ri> where
  $object <fedora-model:hasModel> $model
  and $model <mulgara:is> <info:fedora/usc:test-mezzanine>';
  $results = $connection->repository->ri->itqlQuery($query);
  foreach ($results as $result) {
    usc_mirc_batch_update_object($result['object']['value'], 'usc:mezzanineCModel');
  }
}

function usc_mirc_batch_update_access($connection) {
  $query = 'select $object from <#ri> where
  $object <fedora-model:hasModel> $model
  and $model <mulgara:is> <info:fedora/usc:test-access>';
  $results = $connection->repository->ri->itqlQuery($query);
  foreach ($results as $result) {
    usc_mirc_batch_update_object($result['object']['value'], 'usc:accessCModel');
  }
}

function usc_mirc_batch_update_object($pid, $cmodel) {
  $object = islandora_object_load($pid);
  unset($object->models);
  $object->models = array(
    '0' => $cmodel,
    '1' => 'fedora-system:FedoraObject-3.0',
  );
  watchdog('MIRC@SC update', 'Updated the cmodel for @pid', array('@pid' => $pid), WATCHDOG_INFO);
}
