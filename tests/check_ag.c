/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of libaccounts-glib
 *
 * Copyright (C) 2009-2010 Nokia Corporation.
 *
 * Contact: Alberto Mardegan <alberto.mardegan@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

/**
 * @example check_ag.c
 * Shows how to initialize the framework.
 */

#include "libaccounts-glib/ag-manager.h"
#include "libaccounts-glib/ag-account.h"
#include "libaccounts-glib/ag-errors.h"
#include "libaccounts-glib/ag-internals.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <check.h>
#include <sched.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "config.h"

#define PROVIDER    "dummyprovider"
#define TEST_STRING "Hey dude!"

static gchar *db_filename;
static GMainLoop *main_loop = NULL;
gboolean lock_released = FALSE;
static AgAccount *account = NULL;
static AgManager *manager = NULL;
static AgService *service = NULL;
static gboolean data_stored = FALSE;
static guint source_id = 0;
static guint idle_finish = 0;

typedef struct {
    gboolean called;
    gchar *service;
    gboolean enabled;
} EnabledCbData;

static guint
time_diff(struct timespec *start_time, struct timespec *end_time)
{
    struct timespec diff_time;
    diff_time.tv_sec = end_time->tv_sec - start_time->tv_sec;
    diff_time.tv_nsec = end_time->tv_nsec - start_time->tv_nsec;
    return diff_time.tv_sec * 1000 + diff_time.tv_nsec / 1000000;
}

static void
end_test ()
{
    if (account)
    {
        g_object_unref (account);
        account = NULL;
    }
    if (manager)
    {
        g_object_unref (manager);
        manager = NULL;
    }
    if (service)
    {
        ag_service_unref (service);
        service = NULL;
    }

    if (main_loop)
    {
        g_main_loop_quit (main_loop);
        g_main_loop_unref (main_loop);
        main_loop = NULL;
    }

    data_stored = FALSE;
}

START_TEST(test_init)
{
    g_type_init ();
    manager = ag_manager_new ();

    fail_unless (AG_IS_MANAGER (manager),
                 "Failed to initialize the AgManager.");

    end_test ();
}
END_TEST

START_TEST(test_object)
{
    g_type_init ();
    manager = ag_manager_new ();

    account = ag_manager_create_account (manager, NULL);
    fail_unless (AG_IS_ACCOUNT (account),
                 "Failed to create the AgAccount.");

    end_test ();
}
END_TEST

START_TEST(test_provider)
{
    const gchar *provider_name, *display_name;
    AgProvider *provider;
    GList *providers;

    g_type_init ();
    manager = ag_manager_new ();

    account = ag_manager_create_account (manager, PROVIDER);
    fail_unless (AG_IS_ACCOUNT (account),
                 "Failed to create the AgAccount.");

    provider_name = ag_account_get_provider_name (account);
    fail_if (g_strcmp0 (provider_name, PROVIDER) != 0);

    /* Test provider XML file loading */
    provider = ag_manager_get_provider (manager, "MyProvider");
    fail_unless (provider != NULL);

    display_name = ag_provider_get_display_name (provider);
    fail_unless (g_strcmp0 (display_name, "My Provider") == 0);

    ag_provider_unref (provider);

    /* Test provider enumeration */
    providers = ag_manager_list_providers (manager);
    fail_unless (providers != NULL);
    fail_unless (g_list_length (providers) == 1);
    provider = providers->data;

    display_name = ag_provider_get_display_name (provider);
    fail_unless (g_strcmp0 (display_name, "My Provider") == 0);

    ag_provider_list_free (providers);

    end_test ();
}
END_TEST

void account_store_cb (AgAccount *account, const GError *error,
                       gpointer user_data)
{
    const gchar *string = user_data;

    fail_unless (AG_IS_ACCOUNT (account), "Account got disposed?");
    if (error)
        fail("Got error: %s", error->message);
    fail_unless (g_strcmp0 (string, TEST_STRING) == 0, "Got wrong string");

    end_test ();
}

START_TEST(test_store)
{
    g_type_init ();
    manager = ag_manager_new ();

    account = ag_manager_create_account (manager, PROVIDER);

    main_loop = g_main_loop_new (NULL, FALSE);
    ag_account_store (account, account_store_cb, TEST_STRING);
    if (main_loop)
    {
        g_debug ("Running loop");
        g_main_loop_run (main_loop);
    }
    else
        end_test ();
}
END_TEST

void account_store_locked_cb (AgAccount *account, const GError *error,
                              gpointer user_data)
{
    const gchar *string = user_data;

    g_debug ("%s called", G_STRFUNC);
    fail_unless (AG_IS_ACCOUNT (account), "Account got disposed?");
    if (error)
        fail("Got error: %s", error->message);
    fail_unless (g_strcmp0 (string, TEST_STRING) == 0, "Got wrong string");

    fail_unless (lock_released, "Data stored while DB locked!");

    end_test ();
}

gboolean
release_lock (sqlite3 *db)
{
    g_debug ("releasing lock");
    sqlite3_exec (db, "COMMIT;", NULL, NULL, NULL);
    lock_released = TRUE;
    return FALSE;
}

START_TEST(test_store_locked)
{
    sqlite3 *db;

    g_type_init ();
    manager = ag_manager_new ();

    account = ag_manager_create_account (manager, PROVIDER);

    /* get an exclusive lock on the DB */
    sqlite3_open (db_filename, &db);
    sqlite3_exec (db, "BEGIN EXCLUSIVE", NULL, NULL, NULL);

    main_loop = g_main_loop_new (NULL, FALSE);
    ag_account_store (account, account_store_locked_cb, TEST_STRING);
    g_timeout_add (100, (GSourceFunc)release_lock, db);
    fail_unless (main_loop != NULL, "Callback invoked too early");
    g_debug ("Running loop");
    g_main_loop_run (main_loop);
}
END_TEST

static void
account_store_locked_unref_cb (AgAccount *account, const GError *error,
                               gpointer user_data)
{
    const gchar *string = user_data;

    g_debug ("%s called", G_STRFUNC);
    fail_unless (error != NULL, "Account disposed but no error set!");
    fail_unless (error->code == AG_ERROR_DISPOSED,
                 "Got a different error code");
    fail_unless (g_strcmp0 (string, TEST_STRING) == 0, "Got wrong string");
}

static void
release_lock_unref (sqlite3 *db)
{
    g_debug ("releasing lock");
    sqlite3_exec (db, "COMMIT;", NULL, NULL, NULL);

    end_test ();
}

static gboolean
unref_account (gpointer user_data)
{
    g_debug ("Unreferencing account %p", account);
    fail_unless (AG_IS_ACCOUNT (account), "Account disposed?");
    g_object_unref (account);
    account = NULL;
    return FALSE;
}

START_TEST(test_store_locked_unref)
{
    sqlite3 *db;

    g_type_init ();
    manager = ag_manager_new ();

    account = ag_manager_create_account (manager, PROVIDER);

    /* get an exclusive lock on the DB */
    sqlite3_open (db_filename, &db);
    sqlite3_exec (db, "BEGIN EXCLUSIVE", NULL, NULL, NULL);

    main_loop = g_main_loop_new (NULL, FALSE);
    ag_account_store (account, account_store_locked_unref_cb, TEST_STRING);
    g_timeout_add (10, (GSourceFunc)unref_account, NULL);
    g_timeout_add (20, (GSourceFunc)release_lock_unref, db);
    fail_unless (main_loop != NULL, "Callback invoked too early");
    g_debug ("Running loop");
    g_main_loop_run (main_loop);
}
END_TEST

void account_store_now_cb (AgAccount *account, const GError *error,
                           gpointer user_data)
{
    const gchar *string = user_data;

    fail_unless (AG_IS_ACCOUNT (account), "Account got disposed?");
    if (error)
        fail("Got error: %s", error->message);
    fail_unless (g_strcmp0 (string, TEST_STRING) == 0, "Got wrong string");

    data_stored = TRUE;
}

START_TEST(test_service)
{
    GValue value = { 0 };
    AgService *service2;
    AgAccountId account_id;
    const gchar *provider_name, *service_type, *service_name, *icon_name;
    const gchar *description = "This is really a beautiful account";
    const gchar *username = "me@myhome.com";
    const gint interval = 30;
    const gboolean check_automatically = TRUE;
    const gchar *display_name = "My test account";
    AgSettingSource source;
    GError *error = NULL;

    g_type_init ();

    manager = ag_manager_new ();
    account = ag_manager_create_account (manager, PROVIDER);

    g_value_init (&value, G_TYPE_STRING);
    g_value_set_static_string (&value, description);
    ag_account_set_value (account, "description", &value);
    g_value_unset (&value);

    service = ag_manager_get_service (manager, "MyUnexistingService");
    fail_unless (service == NULL);

    service = ag_manager_get_service (manager, "MyService");
    fail_unless (service != NULL);

    service_type = ag_service_get_service_type (service);
    fail_unless (g_strcmp0 (service_type, "e-mail") == 0,
                 "Wrong service type: %s", service_type);

    service_name = ag_service_get_name (service);
    fail_unless (g_strcmp0 (service_name, "MyService") == 0,
                 "Wrong service name: %s", service_name);

    service_name = ag_service_get_display_name (service);
    fail_unless (g_strcmp0 (service_name, "My Service") == 0,
                 "Wrong service display name: %s", service_name);

    icon_name = ag_service_get_icon_name (service);
    fail_unless (g_strcmp0 (icon_name, "general_myservice") == 0,
                 "Wrong service icon name: %s", icon_name);

    ag_account_set_enabled (account, FALSE);
    ag_account_set_display_name (account, display_name);

    ag_account_select_service (account, service);

    /* test getting default setting from template */
    g_value_init (&value, G_TYPE_INT);
    source = ag_account_get_value (account, "parameters/port", &value);
    fail_unless (source == AG_SETTING_SOURCE_PROFILE,
                 "Cannot get port from profile");
    fail_unless (g_value_get_int (&value) == 5223,
                 "Wrong port number: %d", g_value_get_int (&value));
    g_value_unset (&value);

    /* enable the service */
    ag_account_set_enabled (account, TRUE);

    g_value_init (&value, G_TYPE_STRING);
    g_value_set_static_string (&value, username);
    ag_account_set_value (account, "username", &value);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_BOOLEAN);
    g_value_set_boolean (&value, check_automatically);
    ag_account_set_value (account, "check_automatically", &value);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_INT);
    g_value_set_int (&value, interval);
    ag_account_set_value (account, "interval", &value);
    g_value_unset (&value);

    service2 = ag_manager_get_service (manager, "OtherService");
    ag_account_select_service (account, service2);

    g_value_init (&value, G_TYPE_STRING);
    g_value_set_static_string (&value, "Wednesday");
    ag_account_set_value (account, "day", &value);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_BOOLEAN);
    g_value_set_boolean (&value, TRUE);
    ag_account_set_value (account, "ForReal", &value);
    g_value_unset (&value);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    fail_unless (data_stored, "Callback not invoked immediately");

    g_debug ("Service id: %d", service->id);
    g_debug ("Service2 id: %d", service2->id);
    g_debug ("Account id: %d", account->id);
    account_id = account->id;

    ag_service_unref (service2);
    g_object_unref (account);
    g_object_unref (manager);

    manager = ag_manager_new ();

    /* first, try to load an unexisting account */
    account = ag_manager_load_account (manager, account_id + 2, &error);
    fail_unless (account == NULL, "Loading a non-existing account!");
    fail_unless (error != NULL, "Error is NULL");
    g_clear_error (&error);

    account = ag_manager_load_account (manager, account_id, &error);
    fail_unless (AG_IS_ACCOUNT (account),
                 "Couldn't load account %u", account_id);
    fail_unless (error == NULL, "Error is not NULL");

    provider_name = ag_account_get_provider_name (account);
    fail_unless (g_strcmp0 (provider_name, PROVIDER) == 0,
                 "Got provider %s, expecting %s", provider_name, PROVIDER);

    /* check that the values are retained */
    fail_unless (ag_account_get_enabled (account) == FALSE,
                 "Account enabled!");
    fail_unless (g_strcmp0 (ag_account_get_display_name (account),
                         display_name) == 0,
                 "Display name not retained!");

    g_value_init (&value, G_TYPE_STRING);
    source = ag_account_get_value (account, "description", &value);
    fail_unless (source == AG_SETTING_SOURCE_ACCOUNT, "Wrong source");
    fail_unless (g_strcmp0(g_value_get_string (&value), description) == 0,
                 "Wrong value");
    g_value_unset (&value);

    ag_account_select_service (account, service);

    /* we enabled the service before: check that it's still enabled */
    fail_unless (ag_account_get_enabled (account) == TRUE,
                 "Account service not enabled!");

    g_value_init (&value, G_TYPE_STRING);
    source = ag_account_get_value (account, "username", &value);
    fail_unless (source == AG_SETTING_SOURCE_ACCOUNT, "Wrong source");
    fail_unless (g_strcmp0(g_value_get_string (&value), username) == 0,
                 "Wrong value");
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_BOOLEAN);
    source = ag_account_get_value (account, "check_automatically", &value);
    fail_unless (source == AG_SETTING_SOURCE_ACCOUNT, "Wrong source");
    fail_unless (g_value_get_boolean (&value) == check_automatically,
                 "Wrong value");
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_INT);
    source = ag_account_get_value (account, "interval", &value);
    fail_unless (source == AG_SETTING_SOURCE_ACCOUNT, "Wrong source");
    fail_unless (g_value_get_int (&value) == interval, "Wrong value");
    g_value_unset (&value);

    /* check also value conversion */
    g_value_init (&value, G_TYPE_CHAR);
    source = ag_account_get_value (account, "interval", &value);
    fail_unless (source == AG_SETTING_SOURCE_ACCOUNT, "Wrong source");
    fail_unless (g_value_get_char (&value) == interval, "Wrong value");
    g_value_unset (&value);

    /* change a value */
    g_value_init (&value, G_TYPE_STRING);
    g_value_set_static_string (&value, "Friday");
    ag_account_set_value (account, "day", &value);
    g_value_unset (&value);

    /* change global enabledness */
    ag_account_select_service (account, NULL);
    ag_account_set_enabled (account, TRUE);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    fail_unless (data_stored, "Callback not invoked immediately");

    fail_unless (ag_account_get_enabled (account) == TRUE,
                 "Account still disabled!");
    end_test ();
}
END_TEST

START_TEST(test_account_services)
{
    GList *services;
    AgService *service;

    g_type_init ();
    manager = ag_manager_new ();

    account = ag_manager_create_account (manager, "maemo");
    fail_unless (AG_IS_ACCOUNT (account),
                 "Failed to create the AgAccount.");

    services = ag_account_list_services (account);
    fail_unless (g_list_length (services) == 1);

    service = services->data;
    fail_unless (g_strcmp0 (ag_service_get_name (service), "MyService") == 0);

    ag_service_list_free (services);

    /* check that MyService is returned as a service supporting e-mail for
     * this account */
    services = ag_account_list_services_by_type (account, "e-mail");
    fail_unless (g_list_length (services) == 1);

    service = services->data;
    fail_unless (g_strcmp0 (ag_service_get_name (service), "MyService") == 0);

    ag_service_list_free (services);

    /* check that the account supports the "e-mail" type (it's the type of
     * MyService */
    fail_unless (ag_account_supports_service (account, "e-mail") == TRUE);
    /* and doesn't support "sharing" */
    fail_unless (ag_account_supports_service (account, "sharing") == FALSE);

    end_test ();
}
END_TEST

static void
set_boolean_variable (gboolean *flag)
{
    *flag = TRUE;
}

START_TEST(test_signals)
{
    const gchar *display_name = "My lovely account";
    gboolean enabled_called = FALSE;
    gboolean display_name_called = FALSE;

    g_type_init ();

    manager = ag_manager_new ();
    account = ag_manager_create_account (manager, PROVIDER);

    g_signal_connect_swapped (account, "enabled",
                              G_CALLBACK (set_boolean_variable),
                              &enabled_called);
    g_signal_connect_swapped (account, "display-name-changed",
                              G_CALLBACK (set_boolean_variable),
                              &display_name_called);

    ag_account_set_enabled (account, TRUE);
    ag_account_set_display_name (account, display_name);


    ag_account_store (account, account_store_now_cb, TEST_STRING);
    fail_unless (data_stored, "Callback not invoked immediately");

    fail_unless (enabled_called, "Enabled signal not emitted!");
    fail_unless (display_name_called, "DisplayName signal not emitted!");

    end_test ();
}
END_TEST

START_TEST(test_list)
{
    const gchar *display_name = "New account";
    const gchar *service_name = "OtherService";
    const gchar *service_type;
    GValue value = { 0 };
    AgSettingSource source;
    GList *list;

    g_type_init ();

    manager = ag_manager_new ();
    account = ag_manager_create_account (manager, PROVIDER);

    ag_account_set_enabled (account, TRUE);
    ag_account_set_display_name (account, display_name);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    fail_unless (data_stored, "Callback not invoked immediately");

    fail_unless (account->id != 0, "Account ID is still 0!");

    list = ag_manager_list (manager);
    fail_unless (list != NULL, "Empty list");
    fail_unless (g_list_find (list, GUINT_TO_POINTER (account->id)) != NULL,
                 "Created account not found in list");
    g_list_free (list);

    /* check that it doesn't support the service type provided by MyService */
    service = ag_manager_get_service (manager, service_name);
    service_type = ag_service_get_service_type (service);
    fail_unless (service_type != NULL, "Service %s has no type", service_name);

    list = ag_manager_list_by_service_type (manager, service_type);
    fail_unless (g_list_find (list, GUINT_TO_POINTER (account->id)) == NULL,
                 "New account supports %s service type, but shouldn't",
                 service_type);
    g_list_free (list);

    /* Add the service to the account */
    fail_unless (service != NULL);

    ag_account_select_service (account, service);
    ag_account_set_enabled (account, TRUE);

    /* test getting default setting from template */
    g_value_init (&value, G_TYPE_INT);
    source = ag_account_get_value (account, "parameters/port", &value);
    fail_unless (source == AG_SETTING_SOURCE_PROFILE,
                 "Cannot get port from profile");
    fail_unless (g_value_get_int (&value) == 5223,
                 "Wrong port number: %d", g_value_get_int (&value));
    g_value_unset (&value);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    fail_unless (data_stored, "Callback not invoked immediately");

    /* check that the service is now there */
    list = ag_manager_list_by_service_type (manager, service_type);
    fail_unless (g_list_find (list, GUINT_TO_POINTER (account->id)) != NULL,
                 "New account doesn't supports %s service type, but should",
                 service_type);
    g_list_free (list);

    end_test ();
}
END_TEST

START_TEST(test_settings_iter)
{
    const gchar *keys[] = {
        "param/address",
        "weight",
        "param/city",
        "age",
        "param/country",
        NULL,
    };
    const gchar *values[] = {
        "Helsinginkatu",
        "110",
        "Helsinki",
        "90",
        "Suomi",
        NULL,
    };
    const gchar *service_name = "OtherService";
    GValue value = { 0 };
    AgAccountSettingIter iter;
    const gchar *key;
    const GValue *val;
    gint i, n_values, n_read;
    const gint new_port_value = 432412;

    g_type_init ();

    manager = ag_manager_new ();
    account = ag_manager_create_account (manager, PROVIDER);

    ag_account_set_enabled (account, TRUE);

    for (i = 0; keys[i] != NULL; i++)
    {
        g_value_init (&value, G_TYPE_STRING);
        g_value_set_static_string (&value, values[i]);
        ag_account_set_value (account, keys[i], &value);
        g_value_unset (&value);
    }
    n_values = i;

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    fail_unless (data_stored, "Callback not invoked immediately");

    fail_unless (account->id != 0, "Account ID is still 0!");

    /* iterate the settings */
    n_read = 0;
    ag_account_settings_iter_init (account, &iter, NULL);
    while (ag_account_settings_iter_next (&iter, &key, &val))
    {
        gboolean found = FALSE;
        for (i = 0; keys[i] != NULL; i++)
        {
            if (g_strcmp0 (key, keys[i]) == 0)
            {
                const gchar *text;
                found = TRUE;
                text = g_value_get_string (val);
                fail_unless (g_strcmp0 (values[i], text) == 0,
                             "Got value %s for key %s, expecting %s",
                             text, key, values[i]);
                break;
            }
        }

        fail_unless (found, "Unknown setting %s", key);

        n_read++;
    }

    fail_unless (n_read == n_values,
                 "Not all settings were retrieved (%d out of %d)",
                 n_read, n_values);

    /* iterate settings with prefix */
    n_read = 0;
    ag_account_settings_iter_init (account, &iter, "param/");
    while (ag_account_settings_iter_next (&iter, &key, &val))
    {
        gboolean found = FALSE;
        fail_unless (strncmp (key, "param/", 6) == 0,
                     "Got key %s, wrong prefix", key);
        for (i = 0; keys[i] != NULL; i++)
        {
            if (g_strcmp0 (key, keys[i]) == 0)
            {
                const gchar *text;
                found = TRUE;
                text = g_value_get_string (val);
                fail_unless (g_strcmp0 (values[i], text) == 0,
                             "Got value %s for key %s, expecting %s",
                             text, key, values[i]);
                break;
            }
        }

        fail_unless (found, "Unknown setting %s", key);

        n_read++;
    }

    fail_unless (n_read == 3, "Not all settings were retrieved");

    /* iterate template default settings */
    service = ag_manager_get_service (manager, service_name);
    ag_account_select_service (account, service);
    n_read = 0;
    ag_account_settings_iter_init (account, &iter, NULL);
    while (ag_account_settings_iter_next (&iter, &key, &val))
    {
        g_debug ("Got key %s of type %s", key, G_VALUE_TYPE_NAME (val));

        n_read++;
    }
    fail_unless (n_read == 4, "Not all settings were retrieved");

    /* Add a setting that is also on the template, to check if it will
     * override the one on the template */
    g_value_init (&value, G_TYPE_INT);
    g_value_set_int (&value, new_port_value);
    ag_account_set_value (account, "parameters/port", &value);
    g_value_unset (&value);

    /* Add a setting */
    g_value_init (&value, G_TYPE_STRING);
    g_value_set_static_string (&value, "How's life?");
    ag_account_set_value (account, "parameters/message", &value);
    g_value_unset (&value);

    /* save */
    ag_account_store (account, account_store_now_cb, TEST_STRING);
    fail_unless (data_stored, "Callback not invoked immediately");

    /* enumerate the parameters */
    n_read = 0;
    ag_account_settings_iter_init (account, &iter, "parameters/");
    while (ag_account_settings_iter_next (&iter, &key, &val))
    {
        fail_unless (g_str_has_prefix (key, "parameters/"),
                     "Got key %s, wrong prefix", key);
        g_debug ("Got key %s of type %s", key, G_VALUE_TYPE_NAME (val));
        if (g_strcmp0 (key, "parameters/port") == 0)
        {
            gint port;

            port = g_value_get_int (val);
            fail_unless (port == new_port_value,
                         "Got value %d for key %s, expecting %d",
                         port, key, new_port_value);
        }

        n_read++;
    }

    fail_unless (n_read == 5, "Not all settings were retrieved");


    end_test ();
}
END_TEST

START_TEST(test_list_services)
{
    GList *services, *list;
    gint n_services;
    AgService *service;
    const gchar *name;

    g_type_init ();

    manager = ag_manager_new ();

    /* get all services */
    services = ag_manager_list_services (manager);

    n_services = g_list_length (services);
    fail_unless (n_services == 2, "Got %d services, expecting 2", n_services);

    for (list = services; list != NULL; list = list->next)
    {
        service = list->data;

        name = ag_service_get_name (service);
        g_debug ("Service name: %s", name);
        fail_unless (g_strcmp0 (name, "MyService") == 0 ||
                     g_strcmp0 (name, "OtherService") == 0,
                     "Got unexpected service `%s'", name);
    }
    ag_service_list_free (services);

    /* get services by type */
    services = ag_manager_list_services_by_type (manager, "sharing");

    n_services = g_list_length (services);
    fail_unless (n_services == 1, "Got %d services, expecting 1", n_services);

    list = services;
    service = list->data;
    name = ag_service_get_name (service);
    fail_unless (g_strcmp0 (name, "OtherService") == 0,
                 "Got unexpected service `%s'", name);
    ag_service_list_free (services);

    end_test ();
}
END_TEST

START_TEST(test_delete)
{
    AgAccountId id;
    gboolean enabled_called, deleted_called;

    g_type_init ();

    manager = ag_manager_new ();

    /* create an account */
    account = ag_manager_create_account (manager, PROVIDER);
    ag_account_set_enabled (account, TRUE);
    ag_account_store (account, account_store_now_cb, TEST_STRING);
    fail_unless (data_stored, "Callback not invoked immediately");

    fail_unless (account->id != 0, "Account ID is still 0!");
    id = account->id;

    /* monitor the account status */
    g_signal_connect_swapped (account, "enabled",
                              G_CALLBACK (set_boolean_variable),
                              &enabled_called);
    g_signal_connect_swapped (account, "deleted",
                              G_CALLBACK (set_boolean_variable),
                              &deleted_called);
    enabled_called = deleted_called = FALSE;

    /* delete it */
    ag_account_delete (account);

    /* until ag_account_store() is called, the signals should not have been
     * emitted */
    fail_unless (enabled_called == FALSE, "Accound disabled too early!");
    fail_unless (deleted_called == FALSE, "Accound deleted too early!");

    /* really delete the account */
    ag_account_store (account, account_store_now_cb, TEST_STRING);
    fail_unless (data_stored, "Callback not invoked immediately");

    /* check that the signals are emitted */
    fail_unless (enabled_called, "Accound enabled signal not emitted");
    fail_unless (deleted_called, "Accound deleted signal not emitted");

    g_object_unref (account);

    /* load the account again: this must fail */
    account = ag_manager_get_account (manager, id);
    fail_unless (account == NULL, "The account still exists");

    end_test ();
}
END_TEST

static void
key_changed_cb (AgAccount *account, const gchar *key, gboolean *invoked)
{
    fail_unless (invoked != NULL);
    fail_unless (*invoked == FALSE, "Callback invoked twice!");

    fail_unless (key != NULL);
    fail_unless (g_strcmp0 (key, "parameters/server") == 0 ||
                 g_strcmp0 (key, "parameters/port") == 0,
                 "Callback invoked for wrong key %s", key);
    *invoked = TRUE;
}

static void
dir_changed_cb (AgAccount *account, const gchar *key, gboolean *invoked)
{
    fail_unless (invoked != NULL);
    fail_unless (*invoked == FALSE, "Callback invoked twice!");

    fail_unless (key != NULL);
    fail_unless (g_strcmp0 (key, "parameters/") == 0,
                 "Callback invoked for wrong dir %s", key);
    *invoked = TRUE;
}

START_TEST(test_watches)
{
    gboolean server_changed = FALSE;
    gboolean port_changed = FALSE;
    gboolean dir_changed = FALSE;
    AgAccountWatch w_server, w_port, w_dir;
    GValue value = { 0 };

    g_type_init ();

    manager = ag_manager_new ();
    account = ag_manager_create_account (manager, PROVIDER);

    service = ag_manager_get_service (manager, "MyService");
    fail_unless (service != NULL);

    ag_account_select_service (account, service);

    /* install some watches */
    w_server = ag_account_watch_key (account, "parameters/server",
                                     (AgAccountNotifyCb)key_changed_cb,
                                     &server_changed);
    fail_unless (w_server != NULL);

    w_port = ag_account_watch_key (account, "parameters/port",
                                   (AgAccountNotifyCb)key_changed_cb,
                                   &port_changed);
    fail_unless (w_port != NULL);

    w_dir = ag_account_watch_dir (account, "parameters/",
                                  (AgAccountNotifyCb)dir_changed_cb,
                                  &dir_changed);
    fail_unless (w_port != NULL);

    /* change the port */
    g_value_init (&value, G_TYPE_INT);
    g_value_set_int (&value, 22);
    ag_account_set_value (account, "parameters/port", &value);
    g_value_unset (&value);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    fail_unless (data_stored, "Callback not invoked immediately");

    /* if we didn't change the server, make sure the callback is not
     * invoked */
    fail_unless (server_changed == FALSE, "Callback for 'server' invoked");

    /* make sure the port callback was called */
    fail_unless (port_changed == TRUE, "Callback for 'port' not invoked");

    /* make sure the dir callback was called */
    fail_unless (dir_changed == TRUE, "Callback for 'parameters/' not invoked");


    /* remove the watch for the port */
    ag_account_remove_watch (account, w_port);

    /* change two settings */
    g_value_init (&value, G_TYPE_INT);
    g_value_set_int (&value, 25);
    ag_account_set_value (account, "parameters/port", &value);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_STRING);
    g_value_set_static_string (&value, "warez.maemo.org");
    ag_account_set_value (account, "parameters/server", &value);
    g_value_unset (&value);

    server_changed = FALSE;
    port_changed = FALSE;
    dir_changed = FALSE;
    ag_account_store (account, account_store_now_cb, TEST_STRING);
    fail_unless (data_stored, "Callback not invoked immediately");

    /* make sure the callback for the server is invoked */
    fail_unless (server_changed == TRUE, "Callback for 'server' not invoked");

    /* make sure the port callback was not called (we removed the watch) */
    fail_unless (port_changed == FALSE, "Callback for 'port' invoked");

    /* make sure the dir callback was called */
    fail_unless (dir_changed == TRUE, "Callback for 'parameters/' not invoked");

    end_test ();
}
END_TEST

static void
on_account_created (AgManager *manager, AgAccountId account_id,
                    AgAccountId *id)
{
    g_debug ("%s called (%u)", G_STRFUNC, account_id);

    *id = account_id;
    g_main_loop_quit (main_loop);
}

static void
on_account_deleted (AgManager *manager, AgAccountId account_id,
                    AgAccountId *id)
{
    g_debug ("%s called (%u)", G_STRFUNC, account_id);

    fail_unless (account_id == *id, "Deletion of unexpected account");
    *id = 0;
    g_main_loop_quit (main_loop);
}

static void
changed_cb (AgAccount *account, const gchar *key, gboolean *invoked)
{
    fail_unless (invoked != NULL);
    fail_unless (*invoked == FALSE, "Callback invoked twice!");

    fail_unless (key != NULL);
    *invoked = TRUE;
    if (idle_finish == 0)
        idle_finish = g_idle_add ((GSourceFunc)g_main_loop_quit, main_loop);
}

static gboolean
concurrency_test_failed (gpointer userdata)
{
    g_debug ("Timeout");
    g_main_loop_quit (main_loop);
    source_id = 0;
    return FALSE;
}

static void
on_enabled (AgAccount *account, const gchar *service, gboolean enabled,
            EnabledCbData *ecd)
{
    ecd->called = TRUE;
    ecd->service = g_strdup (service);
    ecd->enabled = ( ag_account_get_enabled (account) == enabled );
}

START_TEST(test_concurrency)
{
    AgAccountId account_id;
    const gchar *provider_name, *display_name;
    gchar command[512];
    GValue value = { 0 };
    gboolean character_changed, string_changed, boolean_changed;
    gboolean unsigned_changed;
    AgSettingSource source;
    EnabledCbData ecd;

    g_type_init ();

    manager = ag_manager_new ();

    g_signal_connect (manager, "account-created",
                      G_CALLBACK (on_account_created), &account_id);

    account_id = 0;
    system ("test-process create myprovider MyAccountName");

    main_loop = g_main_loop_new (NULL, FALSE);
    source_id = g_timeout_add_seconds (2, concurrency_test_failed, NULL);
    g_debug ("Running loop");
    g_main_loop_run (main_loop);

    fail_unless (source_id != 0, "Timeout happened");
    g_source_remove (source_id);

    fail_unless (account_id != 0, "Account ID still 0");

    account = ag_manager_get_account (manager, account_id);
    fail_unless (AG_IS_ACCOUNT (account), "Got invalid account");

    provider_name = ag_account_get_provider_name (account);
    fail_unless (g_strcmp0 (provider_name, "myprovider") == 0,
                 "Wrong provider name '%s'", provider_name);

    display_name = ag_account_get_display_name (account);
    fail_unless (g_strcmp0 (display_name, "MyAccountName") == 0,
                 "Wrong display name '%s'", display_name);

    /* check deletion */
    g_signal_connect (manager, "account-deleted",
                      G_CALLBACK (on_account_deleted), &account_id);
    sprintf (command, "test-process delete %d", account_id);
    system (command);

    source_id = g_timeout_add_seconds (2, concurrency_test_failed, NULL);
    g_main_loop_run (main_loop);
    fail_unless (source_id != 0, "Timeout happened");
    g_source_remove (source_id);

    fail_unless (account_id == 0, "Account still alive");

    /* check a more complex creation */
    system ("test-process create2 myprovider MyAccountName");

    source_id = g_timeout_add_seconds (2, concurrency_test_failed, NULL);
    g_main_loop_run (main_loop);
    fail_unless (source_id != 0, "Timeout happened");
    g_source_remove (source_id);

    fail_unless (account_id != 0, "Account ID still 0");

    account = ag_manager_get_account (manager, account_id);
    fail_unless (AG_IS_ACCOUNT (account), "Got invalid account");

    fail_unless (ag_account_get_enabled (account) == TRUE);

    g_value_init (&value, G_TYPE_INT);
    ag_account_get_value (account, "integer", &value);
    fail_unless (g_value_get_int (&value) == -12345);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_STRING);
    ag_account_get_value (account, "string", &value);
    fail_unless (g_strcmp0 (g_value_get_string (&value), "a string") == 0);
    g_value_unset (&value);

    /* we expect more keys in MyService */
    service = ag_manager_get_service (manager, "MyService");
    fail_unless (service != NULL, "Cannot get service");

    ag_account_select_service (account, service);

    g_value_init (&value, G_TYPE_UINT);
    ag_account_get_value (account, "unsigned", &value);
    fail_unless (g_value_get_uint (&value) == 54321);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_CHAR);
    ag_account_get_value (account, "character", &value);
    fail_unless (g_value_get_char (&value) == 'z');
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_BOOLEAN);
    ag_account_get_value (account, "boolean", &value);
    fail_unless (g_value_get_boolean (&value) == TRUE);
    g_value_unset (&value);

    fail_unless (ag_account_get_enabled (account) == FALSE);

    /* watch some key changes/deletions */
    ag_account_watch_key (account, "character",
                          (AgAccountNotifyCb)changed_cb,
                          &character_changed);

    ag_account_watch_key (account, "boolean",
                          (AgAccountNotifyCb)changed_cb,
                          &boolean_changed);

    ag_account_watch_key (account, "unsigned",
                          (AgAccountNotifyCb)changed_cb,
                          &unsigned_changed);

    ag_account_select_service (account, NULL);
    ag_account_watch_key (account, "string",
                          (AgAccountNotifyCb)changed_cb,
                          &string_changed);
    /* watch account enabledness */
    g_signal_connect (account, "enabled",
                      G_CALLBACK (on_enabled), &ecd);

    character_changed = boolean_changed = string_changed =
        unsigned_changed = FALSE;
    memset (&ecd, 0, sizeof (ecd));

    /* make changes remotely */
    sprintf (command, "test-process change %d", account_id);
    system (command);

    source_id = g_timeout_add_seconds (2, concurrency_test_failed, NULL);
    g_main_loop_run (main_loop);
    fail_unless (source_id != 0, "Timeout happened");
    g_source_remove (source_id);

    fail_unless (character_changed == TRUE);
    fail_unless (boolean_changed == TRUE);
    fail_unless (string_changed == TRUE);
    fail_unless (unsigned_changed == FALSE);

    g_value_init (&value, G_TYPE_STRING);
    ag_account_get_value (account, "string", &value);
    fail_unless (g_strcmp0 (g_value_get_string (&value),
                            "another string") == 0);
    g_value_unset (&value);

    ag_account_select_service (account, service);

    g_value_init (&value, G_TYPE_CHAR);
    source = ag_account_get_value (account, "character", &value);
    fail_unless (source == AG_SETTING_SOURCE_NONE);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_BOOLEAN);
    ag_account_get_value (account, "boolean", &value);
    fail_unless (g_value_get_boolean (&value) == FALSE);
    g_value_unset (&value);

    fail_unless (ag_account_get_enabled (account) == TRUE);

    /* verify that the signal has been emitted correctly */
    fail_unless (ecd.called == TRUE);
    fail_unless (ecd.enabled == TRUE);
    fail_unless (g_strcmp0 (ecd.service, "MyService") == 0);

    end_test ();
}
END_TEST

START_TEST(test_service_regression)
{
    GValue value = { 0 };
    AgAccountId account_id;
    const gchar *provider_name;
    const gchar *username = "me@myhome.com";
    const gint interval = 30;
    const gboolean check_automatically = TRUE;
    const gchar *display_name = "My test account";
    AgSettingSource source;

    /* This test is to catch a bug that happened when creating a new
     * account and settings some service values before setting the display
     * name. The store operation would fail with error "column name is not
     * unique" because for some reason the same service ended up being
     * written twice into the DB.
     */

    /* delete the database: this is essential because the bug was
     * reproducible only when the service was not yet in DB */
    g_unlink (db_filename);

    g_type_init ();

    manager = ag_manager_new ();
    account = ag_manager_create_account (manager, PROVIDER);

    service = ag_manager_get_service (manager, "MyService");
    fail_unless (service != NULL);

    ag_account_select_service (account, service);

    /* enable the service */
    ag_account_set_enabled (account, TRUE);

    g_value_init (&value, G_TYPE_STRING);
    g_value_set_static_string (&value, username);
    ag_account_set_value (account, "username", &value);
    g_value_unset (&value);

    /* Change the display name (this is on the base account) */
    ag_account_set_display_name (account, display_name);

    /* and some more service settings */
    g_value_init (&value, G_TYPE_BOOLEAN);
    g_value_set_boolean (&value, check_automatically);
    ag_account_set_value (account, "check_automatically", &value);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_INT);
    g_value_set_int (&value, interval);
    ag_account_set_value (account, "interval", &value);
    g_value_unset (&value);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    fail_unless (data_stored, "Callback not invoked immediately");

    g_debug ("Service id: %d", service->id);
    g_debug ("Account id: %d", account->id);
    account_id = account->id;

    g_object_unref (account);
    g_object_unref (manager);

    manager = ag_manager_new ();
    account = ag_manager_get_account (manager, account_id);
    fail_unless (AG_IS_ACCOUNT (account),
                 "Couldn't load account %u", account_id);

    provider_name = ag_account_get_provider_name (account);
    fail_unless (g_strcmp0 (provider_name, PROVIDER) == 0,
                 "Got provider %s, expecting %s", provider_name, PROVIDER);

    /* check that the values are retained */
    fail_unless (g_strcmp0 (ag_account_get_display_name (account),
                         display_name) == 0,
                 "Display name not retained!");

    ag_account_select_service (account, service);

    /* we enabled the service before: check that it's still enabled */
    fail_unless (ag_account_get_enabled (account) == TRUE,
                 "Account service not enabled!");

    g_value_init (&value, G_TYPE_STRING);
    source = ag_account_get_value (account, "username", &value);
    fail_unless (source == AG_SETTING_SOURCE_ACCOUNT, "Wrong source");
    fail_unless (g_strcmp0(g_value_get_string (&value), username) == 0,
                 "Wrong value");
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_BOOLEAN);
    source = ag_account_get_value (account, "check_automatically", &value);
    fail_unless (source == AG_SETTING_SOURCE_ACCOUNT, "Wrong source");
    fail_unless (g_value_get_boolean (&value) == check_automatically,
                 "Wrong value");
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_INT);
    source = ag_account_get_value (account, "interval", &value);
    fail_unless (source == AG_SETTING_SOURCE_ACCOUNT, "Wrong source");
    fail_unless (g_value_get_int (&value) == interval, "Wrong value");
    g_value_unset (&value);

    end_test ();
}
END_TEST

START_TEST(test_blocking)
{
    const gchar *display_name, *lock_filename;
    gchar command[512];
    gint timeout_ms, block_ms;
    GError *error = NULL;
    gboolean ok;
    struct timespec start_time, end_time;
    gint fd;

    g_type_init ();

    /* create an account */
    manager = ag_manager_new ();
    account = ag_manager_create_account (manager, PROVIDER);
    fail_unless (account != NULL);
    ag_account_set_display_name (account, "Blocked account");
    ok = ag_account_store_blocking (account, &error);
    fail_unless (ok, "Got error %s", error ? error->message : "No error set");
    fail_unless (account->id != 0);

    display_name = ag_account_get_display_name (account);
    fail_unless (g_strcmp0 (display_name, "Blocked account") == 0,
                 "Wrong display name '%s'", display_name);

    /* Now change the display name and make sure it's not updated
     * without storing :-) */
    ag_account_set_display_name (account, "Want to change");
    display_name = ag_account_get_display_name (account);
    fail_unless (g_strcmp0 (display_name, "Blocked account") == 0);


    /* Now start a process in the background to lock the DB for some time */

    /* first, create a lock file to synchronize the test */
    lock_filename = "/tmp/check_ag.lock";
    fd = open (lock_filename, O_CREAT | O_RDWR, 0666);

    timeout_ms = MAX_SQLITE_BUSY_LOOP_TIME_MS;

    sprintf (command, "test-process lock_db %d %s &",
             timeout_ms, lock_filename);
    system (command);

    /* wait till the file is locked */
    while (lockf(fd, F_TEST, 0) == 0)
        sched_yield();

    clock_gettime (CLOCK_MONOTONIC, &start_time);
    ok = ag_account_store_blocking (account, &error);
    clock_gettime (CLOCK_MONOTONIC, &end_time);

    /* the operation completed successfully */
    fail_unless (ok, "Got error %s", error ? error->message : "No error set");

    /* make sure the display name changed */
    display_name = ag_account_get_display_name (account);
    fail_unless (g_strcmp0 (display_name, "Want to change") == 0);

    /* make sure that we have been waiting for a reasonable time */
    block_ms = time_diff(&start_time, &end_time);
    g_debug ("Been blocking for %u ms", block_ms);

    /* With WAL journaling, the DB might be locked for a much shorter time
     * than what we expect. The following line would fail in that case:
     *
     * fail_unless (block_ms > timeout_ms - 100);
     *
     * Instead, let's just check that we haven't been locking for too long.
     *
     * Note: Increased expected blocking time with 1000 ms ((+ 500) + 1000).
     *       Investigate whether this is needed because of a fault in the accounts-glib
     *       system or not!
     */
    fail_unless (block_ms < timeout_ms + 1500);

    end_test ();
}
END_TEST

#ifdef HAVE_AEGISCRYPTO
START_TEST(test_sign_verify_key)
{
    const gchar *key1 = "test_key/key1";
    const gchar *key2 = "test_key/key2";
    const gchar *list_of_tokens[] = { 
        "libaccounts-glib0::dummy", 
        "libaccounts-glib0::toodummy", 
        "libaccounts-glib0::accounts-glib-access", NULL };
    const gchar *token = "libaccounts-glib0::accounts-glib-access";
    const gchar *reply_token;
    const gchar *data = "some value 1";
    const gchar *data2 = "some value 2";
    gboolean ok;
    GValue value = { 0 };
    
    /* delete the database */
    g_unlink (db_filename);

    g_type_init ();
    
    manager = ag_manager_new ();
    account = ag_manager_create_account (manager, PROVIDER);

    AgService *service;
    service = ag_manager_get_service(manager, "MyService");

    ag_account_set_enabled (account, TRUE);

    g_value_init (&value, G_TYPE_STRING);
    g_value_set_static_string (&value, data);
    ag_account_set_value (account, key1, &value);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_STRING);
    g_value_set_static_string (&value, data2);
    ag_account_set_value (account, key2, &value);
    g_value_unset (&value);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    ok = ag_account_verify (account, key1, &reply_token);
    fail_unless (!ok);

    ok = ag_account_verify_with_tokens (account, key2, list_of_tokens);
    fail_unless (!ok);

    ag_account_sign (account, key1, token);
    ag_account_sign (account, key2, token);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    fail_unless (data_stored, "Callback not invoked immediately");

    fail_unless (account->id != 0, "Account ID is still 0!");
        
    ok = ag_account_verify (account, key1, &reply_token);
    fail_unless (ok);

    ok = ag_account_verify_with_tokens (account, key2, list_of_tokens);
    fail_unless (ok);

    /* testing with other service */
    AgService *service2;
    service2 = ag_manager_get_service(manager, "OtherService");
    ag_account_select_service (account, service2);

    g_value_init (&value, G_TYPE_STRING);
    g_value_set_static_string (&value, data);
    ag_account_set_value (account, key1, &value);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_STRING);
    g_value_set_static_string (&value, data2);
    ag_account_set_value (account, key2, &value);
    g_value_unset (&value);

    ag_account_store (account, account_store_now_cb, TEST_STRING);

    ok = ag_account_verify (account, key1, &reply_token);
    fail_unless (!ok);

    ok = ag_account_verify_with_tokens (account, key2, list_of_tokens);
    fail_unless (!ok);

    ag_account_sign (account, key1, token);
    ag_account_sign (account, key2, token);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    fail_unless (data_stored, "Callback not invoked immediately");

    fail_unless (account->id != 0, "Account ID is still 0!");

    ok = ag_account_verify (account, key1, &reply_token);
    fail_unless (ok);

    ok = ag_account_verify_with_tokens (account, key2, list_of_tokens);

    fail_unless (ok);
    
    end_test ();
}
END_TEST
#endif

START_TEST(test_cache_regression)
{
    AgAccountId account_id1, account_id2;
    const gchar *provider1 = "first_provider";
    const gchar *provider2 = "second_provider";
    const gchar *display_name1 = "first_displayname";
    const gchar *display_name2 = "second_displayname";

    /* This test is to catch a bug that happened when deleting the account
     * with the highest ID without letting the object die, and creating a
     * new account: the account manager would still return the old account!
     */

    /* delete the database */
    g_unlink (db_filename);

    g_type_init ();

    manager = ag_manager_new ();
    account = ag_manager_create_account (manager, provider1);
    fail_unless (account != NULL);

    ag_account_set_display_name (account, display_name1);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    fail_unless (data_stored, "Callback not invoked immediately");

    account_id1 = account->id;

    /* now remove the account, but don't destroy the object */
    ag_account_delete (account);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    fail_unless (data_stored, "Callback not invoked immediately");

    /* after deleting the account, we shouldn't get it anymore, even if we
     * didn't release our reference */
    account = ag_manager_get_account (manager, account_id1);
    fail_unless (account == NULL);

    /* create another account */
    account = ag_manager_create_account (manager, provider2);
    fail_unless (account != NULL);

    ag_account_set_display_name (account, display_name2);

    ag_account_store (account, account_store_now_cb, TEST_STRING);
    fail_unless (data_stored, "Callback not invoked immediately");

    account_id2 = account->id;

    /* check that the values are the correct ones */
    fail_unless (g_strcmp0 (ag_account_get_display_name (account),
                         display_name2) == 0);

    fail_unless (g_strcmp0 (ag_account_get_provider_name (account),
                         provider2) == 0);

    end_test ();
}
END_TEST

START_TEST(test_serviceid_regression)
{
    AgAccount *account1, *account2;
    AgManager *manager1, *manager2;
    AgService *service1, *service2;
    const gchar *provider = "first_provider";

    /* This test is to catch a bug that happened when creating two accounts
     * having the same service, from two different instances of the
     * manager: the creation of the second account would fail.
     * Precondition: empty DB.
     */

    /* delete the database */
    g_unlink (db_filename);

    g_type_init ();

    manager1 = ag_manager_new ();
    manager2 = ag_manager_new ();

    account1 = ag_manager_create_account (manager1, provider);
    fail_unless (account1 != NULL);
    account2 = ag_manager_create_account (manager2, provider);
    fail_unless (account2 != NULL);

    service1 = ag_manager_get_service (manager1, "MyService");
    fail_unless (service1 != NULL);
    service2 = ag_manager_get_service (manager2, "MyService");
    fail_unless (service2 != NULL);

    ag_account_select_service (account1, service1);
    ag_account_set_enabled (account1, TRUE);
    ag_account_select_service (account2, service2);
    ag_account_set_enabled (account2, FALSE);

    ag_account_store (account1, account_store_now_cb, TEST_STRING);
    fail_unless (data_stored, "Callback not invoked immediately");

    ag_account_store (account2, account_store_now_cb, TEST_STRING);
    fail_unless (data_stored, "Callback not invoked immediately");

    fail_unless (account1->id != 0);
    fail_unless (account2->id != 0);

    /* clear up */
    ag_service_unref (service1);
    ag_service_unref (service2);
    g_object_unref (account1);
    g_object_unref (account2);
    g_object_unref (manager1);
    g_object_unref (manager2);

    end_test ();
}
END_TEST

static void
check_enabled (AgAccount *account, const gchar *service, gboolean enabled,
               gboolean *check)
{
    *check = ( ag_account_get_enabled(account) == enabled );
}

START_TEST(test_enabled_regression)
{
    gboolean check = FALSE;

    g_type_init();

    manager = ag_manager_new ();
    account = ag_manager_create_account (manager, PROVIDER);

    fail_unless (account != NULL);

    g_signal_connect (account, "enabled",
                      G_CALLBACK (check_enabled),
                      &check);

    ag_account_set_enabled (account, TRUE);
    ag_account_store (account, NULL, TEST_STRING);

    fail_unless (check, "Settings are not updated!");

    ag_account_set_enabled (account, FALSE);
    ag_account_store (account, NULL, TEST_STRING);

    fail_unless (check, "Settings are not updated!");

    end_test ();
}
END_TEST

START_TEST(test_manager_new_for_service_type)
{
    AgAccount *account1, *account2;
    AgService *service1, *service2;
    AgManager *manager2;
    const gchar *provider = "first_provider";
    GList *list;

    /* delete the database */
    g_unlink (db_filename);

    g_type_init ();

    manager2 = ag_manager_new();
    manager = ag_manager_new_for_service_type ("e-mail");
    fail_unless (g_strcmp0 (ag_manager_get_service_type (manager),
                         "e-mail") == 0);

    account1 = ag_manager_create_account (manager, provider);
    fail_unless (account1 != NULL);
    account2 = ag_manager_create_account (manager, provider);
    fail_unless (account2 != NULL);

    service1 = ag_manager_get_service (manager, "MyService");
    fail_unless (service1 != NULL);
    service2 = ag_manager_get_service (manager, "OtherService");
    fail_unless (service2 != NULL);

    ag_account_set_enabled (account1, TRUE);
    ag_account_select_service (account1, service1);
    ag_account_set_enabled (account1, TRUE);
    ag_account_set_enabled (account2, TRUE);
    ag_account_select_service (account2, service2);
    ag_account_set_enabled (account2, FALSE);

    ag_account_store (account1, account_store_now_cb, TEST_STRING);
    fail_unless (data_stored, "Callback not invoked immediately");
    ag_account_store (account2, account_store_now_cb, TEST_STRING);
    fail_unless (data_stored, "Callback not invoked immediately");

    fail_unless (account1->id != 0);
    fail_unless (account2->id != 0);

    list = ag_manager_list_enabled_by_service_type (manager, "e-mail");
    fail_unless (g_list_length (list) == 1);
    fail_unless (account1->id == GPOINTER_TO_INT(list->data));

    /* clear up */
    ag_service_unref (service1);
    ag_service_unref (service2);
    g_object_unref (account1);
    g_object_unref (account2);
    ag_manager_list_free (list);

    end_test ();
}
END_TEST

static void
on_enabled_event (AgManager *manager, AgAccountId account_id,
                  AgAccountId *id)
{
    g_debug ("%s called (%u)", G_STRFUNC, account_id);
    AgAccount *acc;
    AgService *service;

    acc = ag_manager_get_account (manager, account_id);
    fail_unless (acc != NULL);
    fail_unless (ag_account_get_enabled (acc));

    service = ag_manager_get_service (manager, "MyService");
    fail_unless (service != NULL);
    ag_account_select_service (acc, service);
    fail_unless (ag_account_get_enabled (acc));

    *id = account_id;

    g_main_loop_quit (main_loop);
}

static gboolean
enabled_event_test_failed (gpointer userdata)
{
    g_debug ("Timeout");
    g_main_loop_quit (main_loop);
    source_id = 0;
    return FALSE;
}

START_TEST(test_manager_enabled_event)
{
    g_type_init();

    /* delete the database */
    g_unlink (db_filename);

    /* watch account enabledness */
    gchar command[512];
    AgAccountId source_id;
    AgAccountId account_id = 0;

    manager = ag_manager_new_for_service_type ("e-mail");
    fail_unless (manager != NULL);
    account = ag_manager_create_account (manager, "maemo");
    fail_unless (account != NULL);

    ag_account_set_enabled (account, TRUE);
    ag_account_store (account, account_store_now_cb, TEST_STRING);
    fail_unless (data_stored, "Callback not invoked immediately");

    main_loop = g_main_loop_new (NULL, FALSE);

    g_signal_connect (manager, "enabled-event",
                      G_CALLBACK (on_enabled_event), &account_id);

    /* this command will enable MyService (which is of type e-mail) */
    sprintf (command, "test-process enabled_event %d", account->id);
    system (command);

    source_id = g_timeout_add_seconds (2, enabled_event_test_failed, NULL);
    g_main_loop_run (main_loop);
    fail_unless (source_id != 0, "Timeout happened");
    g_source_remove (source_id);

    fail_unless (account_id == account->id);

    account_id = 0;

    /* now disable the account. This also should trigger the enabled-event. */
    sprintf (command, "test-process enabled_event2 %d", account->id);
    system (command);

    source_id = g_timeout_add_seconds (2, enabled_event_test_failed, NULL);
    g_main_loop_run (main_loop);
    fail_unless (source_id != 0, "Timeout happened");
    g_source_remove (source_id);

    fail_unless (account_id == account->id);

    end_test ();
}
END_TEST

START_TEST(test_list_enabled_account)
{
    GList *list = NULL;
    AgAccount *account1 = NULL;
    AgAccount *account2 = NULL;
    GList *iter = NULL;
    gboolean found = FALSE;
    AgAccount *account3 = NULL;
    const gchar *name = NULL;

    g_type_init ();
    manager = ag_manager_new ();
    fail_unless (manager != NULL, "Manager should not be NULL");

    account1 = ag_manager_create_account (manager, "MyProvider");
    fail_unless (AG_IS_ACCOUNT (account1),
                 "Failed to create the AgAccount.");
    ag_account_set_display_name (account1, "EnabledAccount");
    ag_account_set_enabled (account1, TRUE);
    ag_account_store (account1, account_store_now_cb, TEST_STRING);

    account2 = ag_manager_create_account (manager, "MyProvider");
    fail_unless (AG_IS_ACCOUNT (account2),
                 "Failed to create the AgAccount.");
    ag_account_set_display_name (account2, "DisabledAccount");
    ag_account_set_enabled (account2, FALSE);
    ag_account_store (account2, account_store_now_cb, TEST_STRING);


    list = ag_manager_list_enabled (manager);
    fail_unless (g_list_length (list) > 0,
                 "No enabled accounts?");

    for (iter = list; iter != NULL; iter = g_list_next (iter))
    {
        account3 = ag_manager_get_account (manager,
                                           GPOINTER_TO_UINT (iter->data));

        name = ag_account_get_display_name (account3);
        if (strcmp (name, "EnabledAccount") == 0)
        {
            found = TRUE;
            break;
        }
        g_object_unref (account3);
        account3 = NULL;
    }

    fail_unless (found == TRUE, "Required account not enabled");

    if (account3)
        g_object_unref (account3);
    if (account2)
        g_object_unref (account2);
    if (account1)
        g_object_unref (account1);

    ag_manager_list_free (list);

    end_test ();
}
END_TEST

START_TEST(test_account_list_enabled_services)
{
    GList *services;
    gint n_services;
    AgService *service1, *service2;

    /*
     * Two additional managers:
     * manager2 : e-mail type
     * manager3 : sharing type
     * */
    AgManager *manager2, *manager3;

    /*
     * Same instances of account:
     * account2: from e-mail type manager
     * account3: from sharing manager
     * */
    AgAccount *account2, *account3;

    /*
     * new account for the same manager
     * */
    AgAccount *account4;

    g_type_init ();

    /* delete the database */
    g_unlink (db_filename);

    manager = ag_manager_new ();
    fail_unless (manager != NULL);

    manager2 = ag_manager_new_for_service_type ("e-mail");
    fail_unless (manager2 != NULL);

    manager3 = ag_manager_new_for_service_type ("sharing");
    fail_unless (manager3 != NULL);

    account = ag_manager_create_account (manager, "maemo");
    fail_unless (account != NULL);

    service1 = ag_manager_get_service (manager, "MyService");
    fail_unless (service1 != NULL);
    service2 = ag_manager_get_service (manager, "OtherService");
    fail_unless (service2 != NULL);

    /* 2 services, 1 enabled  */
    ag_account_select_service (account, service1);
    ag_account_set_enabled (account, TRUE);
    ag_account_store (account, account_store_now_cb, TEST_STRING);

    ag_account_select_service (account, service2);
    ag_account_set_enabled (account, FALSE);
    ag_account_store (account, account_store_now_cb, TEST_STRING);

    services = ag_account_list_enabled_services (account);
    n_services = g_list_length (services);
    fail_unless (n_services == 1, "Got %d services, expecting 1", n_services);
    ag_manager_list_free (services);

    /* 2 services, 2 enabled  */
    ag_account_select_service (account, service2);
    ag_account_set_enabled (account, TRUE);
    ag_account_store (account, account_store_now_cb, TEST_STRING);

    services = ag_account_list_enabled_services (account);

    n_services = g_list_length (services);
    fail_unless (n_services == 2, "Got %d services, expecting 2", n_services);
    ag_manager_list_free (services);

    account2 = ag_manager_get_account (manager2, account->id);
    fail_unless (account2 != NULL);

    account3 = ag_manager_get_account (manager3, account->id);
    fail_unless (account3 != NULL);

    services = ag_account_list_enabled_services (account2);

    n_services = g_list_length (services);
    fail_unless (n_services == 1, "Got %d services, expecting 1", n_services);
    ag_manager_list_free (services);

    services = ag_account_list_enabled_services (account3);

    n_services = g_list_length (services);
    fail_unless (n_services == 1, "Got %d services, expecting 1", n_services);
    ag_manager_list_free (services);

    /* 2 services, 0 enabled  */
    account4 = ag_manager_create_account (manager, "maemo");
    fail_unless (account4 != NULL);

    ag_account_select_service (account, service1);
    ag_account_set_enabled (account, FALSE);

    ag_account_select_service (account, service2);
    ag_account_set_enabled (account, FALSE);

    ag_account_store (account, account_store_now_cb, TEST_STRING);

    ag_account_select_service (account4, service2);
    ag_account_set_enabled (account4, TRUE);
    ag_account_store (account4, account_store_now_cb, TEST_STRING);

    services = ag_account_list_enabled_services (account);

    n_services = g_list_length (services);
    fail_unless (n_services == 0, "Got %d services, expecting 0", n_services);
    services = ag_account_list_enabled_services (account);
    /* clear up */
    ag_service_unref (service1);
    ag_service_unref (service2);
    ag_manager_list_free (services);

    g_object_unref (account2);
    g_object_unref (account3);
    g_object_unref (account4);
    g_object_unref (manager2);
    g_object_unref (manager3);

    end_test ();
}
END_TEST

START_TEST(test_service_type)
{
    const gchar *string;
    AgServiceType *service_type;

    g_type_init ();

    manager = ag_manager_new ();

    service_type = ag_manager_load_service_type (manager, "I don't exist");
    fail_unless (service_type == NULL);

    service_type = ag_manager_load_service_type (manager, "e-mail");
    fail_unless (service_type != NULL);

    string = ag_service_type_get_name (service_type);
    fail_unless (g_strcmp0 (string, "e-mail") == 0,
                 "Wrong service type name: %s", string);

    string = ag_service_type_get_display_name (service_type);
    fail_unless (g_strcmp0 (string, "Electronic mail") == 0,
                 "Wrong service type display name: %s", string);

    string = ag_service_type_get_icon_name (service_type);
    fail_unless (g_strcmp0 (string, "email_icon") == 0,
                 "Wrong service type icon name: %s", string);

    string = ag_service_type_get_i18n_domain (service_type);
    fail_unless (g_strcmp0 (string, "translation_file") == 0,
                 "Wrong service type i18n name: %s", string);

    end_test ();
}
END_TEST

static void
on_account_created_with_db_locked (AgManager *manager, AgAccountId account_id)
{
    AgAccount *account;
    AgService *service;
    const gchar *name;
    GList *list;

    g_debug ("%s called (%u)", G_STRFUNC, account_id);

    account = ag_manager_get_account (manager, account_id);
    fail_unless (account != NULL);

    g_debug ("account loaded");
    list = ag_account_list_enabled_services (account);
    fail_unless (list != NULL);
    fail_unless (g_list_length (list) == 1);

    service = list->data;
    fail_unless (service != NULL);
    fail_unless (service->id != 0);

    name = ag_service_get_name (service);
    fail_unless (strcmp (name, "MyService") == 0);

    ag_service_list_free (list);
    g_main_loop_quit (main_loop);
}

START_TEST(test_db_access)
{
    const gchar *lock_filename;
    gchar command[512];
    gint timeout_ms;
    gint fd;

    /* This test is for making sure that no DB accesses occur while certain
     * events occur.
     *
     * Checked scenarios:
     *
     * - when another process creates an account and we get the
     *   account-created signal and call
     *   ag_account_list_enabled_services(), we shouldn't be blocked.
     */
    g_type_init ();

    /* first, create a lock file to synchronize the test */
    lock_filename = "/tmp/check_ag.lock";
    fd = open (lock_filename, O_CREAT | O_RDWR, 0666);

    timeout_ms = 2000; /* two seconds */

    manager = ag_manager_new ();
    ag_manager_set_db_timeout (manager, 0);
    ag_manager_set_abort_on_db_timeout (manager, TRUE);
    g_signal_connect (manager, "account-created",
                      G_CALLBACK (on_account_created_with_db_locked), NULL);

    /* create an account with the e-mail service type enabled */
    system ("test-process create3 myprovider MyAccountName");

    /* lock the DB for the specified timeout */
    sprintf (command, "test-process lock_db %d %s &",
             timeout_ms, lock_filename);
    system (command);

    /* wait till the file is locked */
    while (lockf (fd, F_TEST, 0) == 0)
        sched_yield ();

    /* now the DB is locked; we iterate the main loop to get the signals
     * about the account creation and do some operations with the account.
     * We expect to never get any error because of the locked DB, as the
     * methods we are calling should not access it */

    main_loop = g_main_loop_new (NULL, FALSE);
    source_id = g_timeout_add_seconds (timeout_ms / 1000,
                                       concurrency_test_failed, NULL);
    g_debug ("Running loop");
    g_main_loop_run (main_loop);

    fail_unless (source_id != 0, "Timeout happened");
    g_source_remove (source_id);

    end_test ();
}
END_TEST

Suite *
ag_suite(const char *test_case)
{
    Suite *s = suite_create ("accounts-glib");

#define IF_TEST_CASE_ENABLED(test_name) \
    if (test_case == NULL || strcmp (test_name, test_case) == 0)

    TCase *tc;

    tc = tcase_create("Core");
    tcase_add_test (tc, test_init);
    IF_TEST_CASE_ENABLED("Core")
        suite_add_tcase (s, tc);

    tc = tcase_create("Create");
    tcase_add_test (tc, test_object);
    tcase_add_test (tc, test_provider);
    IF_TEST_CASE_ENABLED("Create")
        suite_add_tcase (s, tc);

    tc = tcase_create("Store");
    tcase_add_test (tc, test_store);
    tcase_add_test (tc, test_store_locked);
    tcase_add_test (tc, test_store_locked_unref);
    IF_TEST_CASE_ENABLED("Store")
        suite_add_tcase (s, tc);

    tc = tcase_create("Service");
    tcase_add_test (tc, test_service);
    tcase_add_test (tc, test_account_services);
    tcase_add_test (tc, test_settings_iter);
    tcase_add_test (tc, test_service_type);
    IF_TEST_CASE_ENABLED("Service")
        suite_add_tcase (s, tc);

    tc = tcase_create("List");
    tcase_add_test (tc, test_list);
    tcase_add_test (tc, test_list_enabled_account);
    tcase_add_test (tc, test_list_services);
    tcase_add_test (tc, test_account_list_enabled_services);
    IF_TEST_CASE_ENABLED("List")
        suite_add_tcase (s, tc);

    tc = tcase_create("Signalling");
    tcase_add_test (tc, test_signals);
    tcase_add_test (tc, test_delete);
    tcase_add_test (tc, test_watches);
    IF_TEST_CASE_ENABLED("Signalling")
        suite_add_tcase (s, tc);

    tc = tcase_create("Concurrency");
    tcase_add_test (tc, test_concurrency);
    tcase_add_test (tc, test_blocking);
#ifdef HAVE_AEGISCRYPTO
    tcase_add_test (tc, test_sign_verify_key);
#endif
    tcase_add_test (tc, test_manager_new_for_service_type);
    tcase_add_test (tc, test_manager_enabled_event);
    /* Tests for ensuring that opening and reading from a locked DB was
     * delayed have been removed since WAL journaling has been introduced:
     * they were failing, because with WAL journaling a writer does not
     * block readers.
     * Should we even need those tests back, they can be found in the git
     * history.
     */
    tcase_set_timeout (tc, 10);
    IF_TEST_CASE_ENABLED("Concurrency")
        suite_add_tcase (s, tc);

    tc = tcase_create("Regression");
    tcase_add_test (tc, test_service_regression);
    tcase_add_test (tc, test_cache_regression);
    tcase_add_test (tc, test_serviceid_regression);
    tcase_add_test (tc, test_enabled_regression);
    IF_TEST_CASE_ENABLED("Regression")
        suite_add_tcase (s, tc);

    tc = tcase_create("Caching");
    tcase_add_test (tc, test_db_access);
    tcase_set_timeout (tc, 10);
    IF_TEST_CASE_ENABLED("Caching")
        suite_add_tcase (s, tc);

    return s;
}

int main(int argc, char **argv)
{
    int number_failed;
    const char *test_case = NULL;

    if (argc > 1)
        test_case = argv[1];

    Suite * s = ag_suite(test_case);
    SRunner * sr = srunner_create(s);

    db_filename = g_build_filename (g_getenv ("ACCOUNTS"), "accounts.db",
                                    NULL);

    srunner_set_xml(sr, "/tmp/result.xml");
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free (sr);

    g_free (db_filename);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* vim: set ai et tw=75 ts=4 sw=4: */

