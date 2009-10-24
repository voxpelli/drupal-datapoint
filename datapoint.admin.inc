<?php
// $Id$

/**
 * @file
 * Admin functionality for the Datapoint module
 */

function _datapoint_refresh_source($sid) {
  set_time_limit(0);

  //Update the single source
  datapoint_update_source($sid);

  // Update and create nodes.
  datapoint_update_nodes();

  // Clean up sources.
  module_invoke_all('datapoint_cleanup');

  $source = datapoint_source($sid);
  drupal_set_message(t('Forced a refresh of the %source source.', array('%source' => $source['name'])));

  drupal_goto('admin/content/datapoint');
}

function _datapoint_admin_index_page() {
  global $pager_total, $pager_page_array;

  $dest = array('query' => array('destination' => $_GET['q']));
  $page = isset($_GET['page'])?$_GET['page']:0;
  $total = db_result(db_query('SELECT COUNT(sid) FROM {datapoint_source}'));
  $page_size = 20;

  $pager_page_array[0] = $page;
  $pager_total[0] = $total;

  $res = db_query_range("SELECT ds.sid, ds.source_type, ds.name, ds.pinged, ds.last_update, ds.managed, COUNT(d.pid) as dirty FROM {datapoint_source} ds
    LEFT JOIN {datapoint} d ON d.sid = ds.sid AND d.dirty = 1
    GROUP BY ds.sid, ds.source_type, ds.name, ds.pinged, ds.last_update, ds.managed
    ORDER BY name, managed", $page * $page_size, $page_size);
  $header = array(t('Name'), t('Id'), t('Type'), t('Updated'), t('Queued'), array('data' => t('Operations'), 'colspan' => '3'));
  $rows = array();

  if ($total>$page_size) {
    $out = theme('pager', $page_size);
  }

  while ($row = db_fetch_object($res)) {
    $rows[] = array(
      $row->name . ($row->managed?(' ' . t('(managed)')):''),
      $row->sid,
      l($row->source_type, 'admin/content/datapoint/source-types/' . $row->source_type, $dest),
      ($row->last_update ? format_date($row->last_update) : t('Not updated')) .
        ($row->pinged?(' ' . t('(pinged)')):''),
      $row->dirty,
      l(t('Edit'), 'admin/content/datapoint/source/' . $row->sid, $dest),
      l(t('Refresh'), 'admin/content/datapoint/source/refresh/' . $row->sid, $dest),
      l(t('Export'), 'admin/content/datapoint/source/export/' . $row->sid, $dest),
    );
  }
  $out .= theme('table', $header, $rows);

  if ($total>$page_size) {
    $out .= theme('pager', $page_size);
  }

  return $out;
}

function _datapoint_admin_source_type_list() {
  $header = array(
    array('data' => t('Name'),          'class' => 'datapoint-source-type-name'),
    array('data' => t('Parser'),        'class' => 'datapoint-source-type-parser'),
    array('data' => t('Creates nodes'), 'class' => 'datapoint-source-type-content-type'),
    array('data' => t('Storage'),       'class' => 'datapoint-source-type-storage'),
  );

  $header[] = array('data' => t('Operations'), 'class' => 'datapoint-source-type-operations');

  $dest          = array('query' => array('destination' => $_GET['q']));
  $source_types  = datapoint_source_type_load_all();
  $rows          = array();

  foreach ($source_types as $source_type) {
    $operations = array();

    $operations[] = array(
      'title' => t('Edit'),
      'href'  => 'admin/content/datapoint/source-types/' . $source_type->typename,
    ) + $dest;
    $operations[] = array(
      'title' => t('Export'),
      'href'  => 'admin/content/datapoint/source-types/' . $source_type->typename . '/export',
    ) + $dest;

    if ($source_type->export_type == (EXPORT_IN_CODE | EXPORT_IN_DATABASE)) {
      $operations[] = array(
        'title' => t('Revert'),
        'href'  => 'admin/content/datapoint/source-types/' . $source_type->typename . '/delete',
      );
    }
    elseif ($source_type->export_type != EXPORT_IN_CODE) {
      //TODO: Make it possible to delete it
      /*$operations[] = array(
        'title' => t('Delete'),
        'href'  => 'admin/content/datapoint/source-types/' . $source_type->typename . '/delete',
      );*/
    }

    $rows[$source_type->typename] = array(
      'data' => array(
        'name' => array(
          'data'  => $source_type->name,
          'class' => 'datapoint-source-type-name',
        ),
        'preset' => array(
          'data'  => $source_type->configuration['preset'],
          'class' => 'datapoint-source-type-preset',
        ),
        'content_type' => array(
          'data'  => $source_type->configuration['content_type'] ? $source_type->configuration['content_type'] : '<b>' . t('No') . '</b>',
          'class' => 'datapoint-source-type-content-type',
        ),
        'storage' => array(
          'data'  => ($source_type->export_type == EXPORT_IN_CODE) ? t('In code') : t('In database'),
          'class' => 'datapoint-source-type-storage',
        ),
        'operations' => array(
          'data'  => theme('links', $operations),
          'class' => 'datapoint-source-type-operations',
        ),
      ),
      'class' => 'datapoint-source-type-' . $source_type->typename,
      //'title => ??
    );
  }

  $table = theme('table', $header, $rows, array('id' => 'datapoint-source-type-list'));

  return $table;
}

function _datapoint_admin_add_source_type($state) {
  //TODO: Perhaps use a ctools backed datapoint_source_type_news() function?
  //Build the parser options
  $modules = module_implements('datapoint_parser');
  $parsers = array('' => t('-- Select a data parser'));
  foreach ($modules as $module) {
    $info = call_user_func($module . '_datapoint_parser');
    $parsers[$module] = $info['name'];
  }

  //Build the node type options
  $node_types = node_get_types();
  $node_type_options = array('' => t('-- Select a node type'));
  foreach ($node_types as $type => $type_info) {
    $node_type_options[$type] = $type_info->name;
  }

  $form = array(
    '#type' => 'fieldset',
    '#title' => t('Add source type'),
  );

  $form['parser_type'] = array(
    '#type' => 'select',
    '#title' => t('Parser'),
    '#required' => TRUE,
    '#options' => $parsers,
    '#description' => t('This is the parser that will be used for the source'),
  );

  $form['content_type'] = array(
    '#type' => 'select',
    '#title' => t('Node type'),
    '#options' => $node_type_options,
    '#description' => t('Select a node type that will be used to publish data points'),
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Add'),
  );

  return $form;
}

function _datapoint_admin_add_source_type_configure($form_state, $parser, $content_type = NULL, $type = NULL) {
  $form = array(
    'parser' => array(
      '#type' => 'value',
      '#value' => $parser,
    ),
    'content_type' => array(
      '#type' => 'value',
      '#value' => $content_type,
    ),
  );

  if ($type) {
    $form['typename'] = array(
      '#type' => 'value',
      '#value' => $type->typename,
    );
    $form['display_type'] = array(
      '#type' => 'item',
      '#title' => t('Type'),
      '#value' => $type->typename,
    );
  }
  else {
    $form['typename'] = array(
      '#type' => 'textfield',
      '#title' => t('Type'),
      '#required' => TRUE,
      '#description' => t('The machine-readable name of this source type. This name must contain only lowercase letters, numbers, and underscores. This name must be unique.'),
      '#default_value' => '',
    );
  }

  $form['name'] = array(
    '#type' => 'textfield',
    '#title' => t('Name'),
    '#required' => TRUE,
    '#description' => t('The human-readable name of this source type.'),
    '#default_value' => $type ? $type->name : '',
  );

  if ($content_type) {
    // Build the data point field options
    $dp_fields = datapoint_source_fields($parser);
    $dp_field_options = array_merge(array('' => t('-- Empty')));
    foreach ($dp_fields as $name => $def) {
      $dp_field_options[$name] = $def['description'];
    }

    $fields = datapoint_managed_fields($content_type);

    $form['field_mappings'] = array(
      '#type' => 'fieldset',
      '#title' => t('Field mappings'),
    );

    foreach ($fields as $name => $finfo) {
      $form['field_mappings']['field_source_' . $name] = array(
        '#type' => 'select',
        '#title' => $finfo['label'],
        '#required' => isset($finfo['required']) && $finfo['required'],
        '#options' => $dp_field_options,
        '#default_value' => $type ? $type->configuration['fields'][$name]['source'] : '',
        '#ahah' => array(
          'method' => 'replace',
          'wrapper' => 'field-source-' . $name . '-configuration-placeholder',
          'path' => 'datapoint/ahah/field-source-configuration/' . $parser . '/'  . $name,
        ),
      );

      $source = $type->configuration['fields'][$name]['source'];
      if ($type && !empty($source) && is_callable($dp_fields[$source]['configuration_form'])) {
        $conf = call_user_func($dp_fields[$source]['configuration_form'], $form_state, $name);
      }
      else {
        $conf = array(
          '#type' => 'markup',
          '#value' => '<span></span>',
        );
      }

      $conf = array_merge($conf, array(
        '#prefix' => '<div id="field-source-' . $name . '-configuration-placeholder">',
        '#suffix' => '</div>',
      ));

      $form['field_mappings']['field_source_' . $name . '_configuration_placeholder'] = $conf;
    }

    // Simulate a node form to get the taxonomy module to provide taxonomy fields
    $tax = array();
    $tax['type'] = array('#type' => 'value', '#value' => $content_type);
    $tax['#node'] = (object)array(
      'nid' => 0,
      'type' => $content_type,
    );
    taxonomy_form_alter($tax, array(), $content_type . '_node_form');
    $form['taxonomy'] = $tax['taxonomy'];
    $form['taxonomy']['#weight'] = 90;
    $form['taxonomy']['#description'] = t('Set the default categories and tags that should be used for nodes generated from sources of this type. This can be overridden on individual sources later.');
  }

  $form['submit'] = array(
    '#weight' => 99,
    '#type' => 'submit',
    '#value' => t('Save'),
  );

  return $form;
}

function _datapoint_admin_add_source_type_configure_validate($form, &$form_state) {
  $source_type = datapoint_source_type_configuration_from_form_values($form_state['values']);
  if (!preg_match('!^[a-z0-9_]+$!', $source_type->typename)) {
    form_set_error('typename', t('The machine-readable name must contain only lowercase letters, numbers, and underscores.'));
  }
  if (datapoint_source_type_load($source_type->typename)) {
    form_set_error('typename', t('The machine-readable name %type is already taken.', array('%type' => $source_type->typename)));
  }
}

function _datapoint_admin_add_source_type_configure_submit($form, $form_state) {
  $source_type = datapoint_source_type_configuration_from_form_values($form_state['values']);
  datapoint_source_type_save($source_type);

  drupal_set_message(t('The datapoint source type "!name" has been added', array(
    '!name' => $source_type->name,
  )));
  drupal_goto('admin/content/datapoint/source-types');
}

function datapoint_source_type_configuration_from_form_values($values) {
  $parser = $values['parser'];
  $content_type = $values['content_type'];

  $source_type = (object)array(
    'typename' => $values['typename'],
    'name'     => $values['name'],
    'configuration' => array(
      'content_type' => $content_type,
      'parser'       => $parser,
    ),
  );

  if ($content_type) {
    $source_type->configuration['fields'] = array();
    $source_type->configuration['taxonomy'] = taxonomy_preview_terms((object)array('taxonomy' => $values['taxonomy']));

    $fields = datapoint_managed_fields($content_type);
    $source_fields = datapoint_source_fields($parser);
    foreach ($fields as $name => $def) {
      if (!empty($values['field_source_' . $name])) {
        $source_name = $values['field_source_' . $name];
        $source = $source_fields[$source_name];

        $field_conf = array(
          'source' => $source_name,
        );

        if (!empty($source['configuration_form'])) {
          $field_conf['configuration'] = call_user_func($source['configuration_form'] . '_submit', $name, $values);
        }

        $source_type->configuration['fields'][$name] = $field_conf;
      }
    }
  }

  return $source_type;
}

function datapoint_ahah_field_source_configuration($parser, $field) {
  $cached_form_state = array();
  $cached_form = form_get_cache($_POST['form_build_id'], $cached_form_state);
  $dp_fields = datapoint_source_fields($parser);

  $source = $_POST['field_source_' . $field];
  if (empty($source) || empty($dp_fields[$source]['configuration_form'])) {
    $set = array(
      '#value' => '<span></span>',
    );
  }
  else {
    $set = call_user_func($dp_fields[$source]['configuration_form'], $cached_form_state, $field);
  }

  $cached_form['field_source_' . $field . '_configuration'] = $set;
  form_set_cache($_POST['form_build_id'], $cached_form, $cached_form_state);
  $form_state = array('submitted' => FALSE);
  $set = form_builder('datapoint_ahah_field_source_configuration', $set, $form_state);
  $output = drupal_render($set);
  print drupal_to_js(array('status' => TRUE, 'data' => $output));
  exit;
}

function _datapoint_admin_add_source_type_submit($form, $form_state) {
  $values = $form_state['values'];
  $path   = 'admin/content/datapoint/source-types/add/' . $values['parser_type'];
  if (!empty($values['content_type'])) {
    $path .= '/' . $values['content_type'];
  }
  drupal_goto($path);
}

function _datapoint_source_add() {
  $form = array();
  drupal_add_js('misc/autocomplete.js');

  $modules = module_implements('datapoint');
  $source_types = array_merge(array(
    0 => t('-- Select a source type'),
  ), datapoint_source_type_options_list());
  foreach ($modules as $module) {
    $info = call_user_func($module . '_datapoint');
    $source_types[$module] = $info['name'];
  }

  $form['source_name'] = array(
    '#type' => 'textfield',
    '#title' => t('Name'),
    '#default_value' => '',
    '#required' => TRUE,
  );

  $form['source_type'] = array(
    '#type' => 'select',
    '#title' => t('Source type'),
    '#options' => $source_types,
    '#default_value' => 0,
    '#ahah' => array(
      'method' => 'replace',
      'wrapper' => 'source-configuration',
      'path' => 'datapoint/ahah/source_configuration'
    ),
  );

  $form['source_configuration_placeholder'] = array(
    '#type' => 'markup',
    '#prefix' => '<div id="source-configuration">',
    '#suffix' => '</div>',
    '#value' => '<span></span>',
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#weight' => 99,
    '#value' => t('Save'),
  );

  return $form;
}

function datapoint_source_configuration_from_form_values($values) {
  $configuration = array(
    'source_type' => $values['source_type'],
    'name' => $values['source_name'],
    'taxonomy' => taxonomy_preview_terms((object)$values),
  );

  $type = datapoint_source_type_load($values['source_type']);
  $configuration['parser'] = $type->configuration['parser'];

  $info = datapoint_parser_info($configuration['parser']);
  if (isset($info['source configuration submit'])) {
    $configuration['source configuration'] = call_user_func($info['source configuration submit'], $values);
  }
  return $configuration;
}

function _datapoint_source_add_submit($form, $form_state) {
  $values = $form_state['values'];
  $configuration = datapoint_source_configuration_from_form_values($values);
  datapoint_add_source($configuration);
}

function datapoint_source_configuration_form($type, $configuration) {
  $info = datapoint_parser_info($type->configuration['parser']);
  if (isset($info['source configuration'])) {
    $form = call_user_func($info['source configuration'], array());
  }
  else {
    $form = array();
  }

  // Simulate a node form to get the taxonomy module to alter our form
  $form['type']  = array(
    '#type'  => 'value',
    '#value' => $type->configuration['content_type'],
  );
  $form['#node'] = (object)array(
    'nid'      => 0,
    'type'     => $type->configuration['content_type'],
    'taxonomy' => $type->configuration['taxonomy'],
  );
  taxonomy_form_alter($form, array(), $type->configuration['content_type'] . '_node_form');

  //Remove the attributes needed for taxonomy_form_alter
  unset($form['type']);
  unset($form['#node']);
  return $form;
}

function datapoint_ahah_source_configuration() {
  $cached_form_state = array();
  $cached_form = form_get_cache($_POST['form_build_id'], $cached_form_state);

  $type_name = $_POST['source_type'];
  if (!empty($type_name)) {
    $type = datapoint_source_type_load($type_name);
    $set = datapoint_source_configuration_form($type, array());
  }
  else {
    $set = array(
      '#value' => '<span></span>',
    );
  }

  $cached_form['source_configuration'] = $set;
  form_set_cache($_POST['form_build_id'], $cached_form, $cached_form_state);
  $form_state = array('submitted' => FALSE);
  $set = form_builder('datapoint_source_configuration_ahah', $set, $form_state);
  $output = drupal_render($set);
  print drupal_to_js(array('status' => TRUE, 'data' => $output));
  exit;
}

function _datapoint_admin_edit_source_type($form_state, $source_type) {
  if (!is_object($source_type)) {
    $source_type = datapoint_source_type_load($source_type);
  }
  return _datapoint_admin_add_source_type_configure($form_state, $source_type->configuration['parser'], $source_type->configuration['content_type'], $source_type);
}

function _datapoint_admin_edit_source_type_submit($form, $form_state) {
  $source_type = datapoint_source_type_configuration_from_form_values($form_state['values']);
  $current     = datapoint_source_type_load($source_type->typename);
  $update      = $current->export_type != EXPORT_IN_CODE;

  datapoint_source_type_save($source_type, $update);

  drupal_set_message(t('The datapoint source type "!name" has been updated', array(
    '!name' => $source_type->name,
  )));
  drupal_goto('admin/content/datapoint/source-types');
}

/**
 * Provide a form to confirm deletion of a source type.
 */
function datapoint_delete_confirm_source_type(&$form_state, $source_type) {
  if (!is_object($source_type)) {
    $source_type = datapoint_source_type_load($source_type);
  }
  if ($source_type->export_type == (EXPORT_IN_CODE | EXPORT_IN_DATABASE)) {
    $submit = t('revert');
  }
/*  elseif ($preset->export_type != EXPORT_IN_CODE) {
    $submit = t('delete');
  }*/
  else {
    drupal_not_found();
    die;
  }
  $form['typename'] = array('#type' => 'value', '#value' => $source_type->typename);
  return confirm_form($form,
    t('Are you sure you want to !delete the source type "@name"?', array('!delete' => $submit, '@name' => $source_type->name)),
    !empty($_GET['destination']) ? $_GET['destination'] : 'admin/content/datapoint/source-types',
    t('This action cannot be undone.'),
    $submit, t('Cancel')
  );
}

/**
 * Handle the submit button to delete a mini panel.
 */
function datapoint_delete_confirm_source_type_submit($form, &$form_state) {
  $source_type = datapoint_source_type_load($form_state['values']['typename']);
  datapoint_source_type_delete($source_type);
  $form_state['redirect'] = 'admin/content/datapoint/source-types';
}

function _datapoint_admin_edit_source($form_state, $sid) {
  $source = datapoint_source($sid);
  drupal_set_title($source['name']);

  $form = array(
    'sid' => array(
      '#type' => 'value',
      '#value' => $sid,
    ),
  );

  $form['source_type'] = array(
    '#type' => 'item',
    '#title' => t('Source type'),
    '#value' => l($source['source_type'], 'admin/content/datapoint/source-types/' . $source['source_type']),
  );

  $form['managed'] = array(
    '#type' => 'item',
    '#title' => t('Managed'),
    '#value' => $source['managed']?t('Yes'):t('No'),
  );

  $source_type = datapoint_source_type_load($source['source_type']);
  $info = datapoint_parser_info($source_type->configuration['parser']);
  if (isset($info['source configuration'])) {
    $conf_form = call_user_func($info['source configuration'], $source['source configuration']);
  }
  else {
    $conf_form = array();
  }

  $form['configuration'] = $conf_form;

  $form['last_update'] = array(
    '#type' => 'item',
    '#title' => t('Last update'),
    '#value' => $source['last_update'] ? format_date($source['last_update']) : t('Not updated yet.'),
  );

  _datapoint_recursively_disable_form($form);

  $form['pinged'] = array(
    '#type' => 'checkbox',
    '#title' => t('Pinged'),
    '#default_value' => $source['pinged'],
    '#description' => t('Check the pinged-box to prioritize the source at next cron run.')
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
  );
  return $form;
}

function _datapoint_admin_edit_source_submit($form, $form_state) {
  $values = $form_state['values'];
  $source = datapoint_source($values['sid']);

  // Only allow updates of pinged-state when the source is managed
  if ($source['managed']) {
    datapoint_save_source(array(
      'sid' => $values['sid'],
      'pinged' => $values['pinged'],
    ));
  }
  else {
    $info = datapoint_parser_info($source['parser']);
    if (isset($info['source configuration submit'])) {
      $configuration = call_user_func($info['source configuration submit'], $values);
    }
    else {
      $configuration = array();
    }
    $source['source configuration'] = $configuration;
    datapoint_save_source($source);
  }

  drupal_set_message(t('The datapoint source "!name" was updated', array('!name' => $source['name'])));
}

function _datapoint_recursively_disable_form(&$form) {
  foreach ($form as $key => &$def) {
    if (drupal_substr($key, 0, 1) !== '#') {
      $def['#disabled'] = TRUE;
      _datapoint_recursively_disable_form($def);
    }
  }
}

function datapoint_delete_source_confirm($form_state, $sid) {
  $source = datapoint_source($sid);

  if ($source) {
    $form['sid'] = array(
      '#type' => 'value',
      '#value' => $sid,
    );

    return confirm_form($form,
      t('Are you sure you want to delete %name?', array('%name' => $node['name'])),
      isset($_GET['destination']) ? $_GET['destination'] : 'admin/content/datapoint',
      t('This action cannot be undone.'),
      t('Delete'),
      t('Cancel')
    );
  }
  else {
    drupal_not_found();
  }
}

function datapoint_delete_source_confirm_submit($form, &$form_state) {
  if ($form_state['values']['confirm']) {
    datapoint_delete_source($form_state['values']['sid']);
  }

  $form_state['redirect'] = 'admin/content/datapoint';
}

function datapoint_export_source($form_state, $sid) {
  $source = datapoint_source($sid);

  if ($source) {
    drupal_set_title(t('Export of !name', array('!name' => $source['name'])));
    drupal_add_css(drupal_get_path('module', 'datapoint') . '/datapoint.css');
    unset($source['configuration']);

    return _datapoint_export_form($source);
  }
  else {
    drupal_not_found();
  }
}

/**
 * Page callback to export a source type to PHP code.
 */
function datapoint_export_source_type(&$form_state, $source_type) {
  //TODO: perhaps remove _datapoint_export_form which was used by this function?
  if (!is_object($source_type)) {
    $source_type = datapoint_source_type_load($source_type);
  }
  if ($source_type) {
    drupal_set_title(check_plain($source_type->name));
    $code = datapoint_source_type_export($source_type);

    $lines = substr_count($code, "\n");
    $form['code'] = array(
      '#type'          => 'textarea',
      '#title'         => $source_type->name,
      '#default_value' => $code,
      '#rows'          => $lines,
    );

    return $form;
  }
  else {
    drupal_not_found();
  }
}

function _datapoint_export_form($data) {
  $form = array(
    'top_submit' => NULL,
    'datapoint_export' => array(
      '#type' => 'textarea',
      '#rows' => 20,
      '#cols' => 40,
      '#default_value' => var_export($data, TRUE),
      '#attributes' => array(
        'class' => 'datapoint-export',
      ),
    ),
  );

  if (!empty($_GET['destination'])) {
    $form['top_submit'] = $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Ok'),
    );
  }

  return $form;
}

/**
 * Marks the source as pinged and gives it priority during the next update
 *
 * @return void
 */
function datapoint_ping($sid) {
  db_query("UPDATE {datapoint_source} SET pinged=1 WHERE sid=%d", $sid);
}