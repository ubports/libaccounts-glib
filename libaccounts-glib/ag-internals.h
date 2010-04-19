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

#ifndef _AG_INTERNALS_H_
#define _AG_INTERNALS_H_

#include "ag-manager.h"
#include <dbus/dbus.h>
#include <sqlite3.h>
#include <time.h>

G_BEGIN_DECLS

#define AG_DBUS_PATH "/com/nokia/AccountsLib"
#define AG_DBUS_IFACE "com.nokia.AccountsLib"
#define AG_DBUS_SIG_CHANGED "AccountChanged"

typedef struct _AgAccountChanges AgAccountChanges;

struct _AgAccountChanges {
    gboolean deleted;
    gboolean created;

    /* The keys of the table are service names, and the values are
     * AgServiceChanges structures */
    GHashTable *services;
};

G_GNUC_INTERNAL
void _ag_account_store_completed (AgAccount *account,
                                  AgAccountChanges *changes,
                                  AgAccountStoreCb callback,
                                  const GError *error, gpointer user_data);

G_GNUC_INTERNAL
void _ag_account_done_changes (AgAccount *account, AgAccountChanges *changes);

G_GNUC_INTERNAL
DBusMessage *_ag_account_build_signal (AgAccount *account,
                                       AgAccountChanges *changes,
                                       const struct timespec *ts);
G_GNUC_INTERNAL
AgAccountChanges *_ag_account_changes_from_dbus (DBusMessageIter *iter,
                                                 gboolean created,
                                                 gboolean deleted);

void _ag_manager_exec_transaction (AgManager *manager, const gchar *sql,
                                   AgAccountChanges *changes,
                                   AgAccount *account,
                                   AgAccountStoreCb callback,
                                   gpointer user_data) G_GNUC_INTERNAL;

typedef gboolean (*AgQueryCallback) (sqlite3_stmt *stmt, gpointer user_data);

G_GNUC_INTERNAL
void _ag_manager_exec_transaction_blocking (AgManager *manager,
                                            const gchar *sql,
                                            AgAccountChanges *changes,
                                            AgAccount *account,
                                            GError **error);
G_GNUC_INTERNAL
gint _ag_manager_exec_query (AgManager *manager,
                             AgQueryCallback callback, gpointer user_data,
                             const gchar *sql);

struct _AgService {
    /*< private >*/
    gint ref_count;
    gchar *name;
    gchar *display_name;
    gchar *type;
    gchar *provider;
    gchar *icon_name;
    gchar *file_data;
    gsize type_data_offset;
    gint id;
    GHashTable *default_settings;
};

G_GNUC_INTERNAL
GList *_ag_services_list (AgManager *manager);

G_GNUC_INTERNAL
AgService *_ag_service_new_from_file (const gchar *service_name);

G_GNUC_INTERNAL
GHashTable *_ag_service_load_default_settings (AgService *service);

G_GNUC_INTERNAL
const GValue *_ag_service_get_default_setting (AgService *service,
                                               const gchar *key);

inline
AgService *_ag_service_new (void);

struct _AgProvider {
    /*< private >*/
    gint ref_count;
    gchar *name;
    gchar *display_name;
    gchar *file_data;
};

G_GNUC_INTERNAL
GList *_ag_providers_list (AgManager *manager);

G_GNUC_INTERNAL
AgProvider *_ag_provider_new_from_file (const gchar *provider_name);

G_GNUC_INTERNAL
gboolean _ag_account_changes_have_enabled (AgAccountChanges *changes);

G_GNUC_INTERNAL
GList *_ag_manager_list_all (AgManager *manager);

G_GNUC_INTERNAL
void _ag_account_changes_free (AgAccountChanges *change);
#endif /* _AG_INTERNALS_H_ */
