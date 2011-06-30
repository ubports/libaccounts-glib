/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of libaccounts-glib
 *
 * Copyright (C) 2011 Nokia Corporation.
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

#include "ag-account-service.h"
#include "ag-errors.h"
#include "ag-internals.h"
#include "ag-manager.h"
#include "ag-service-type.h"

enum {
    PROP_0,

    PROP_ACCOUNT,
    PROP_SERVICE
};

struct _AgAccountServicePrivate {
    AgAccount *account;
    AgService *service;
    gboolean enabled;
    AgAccountWatch watch;
    guint account_enabled_id;
};

enum
{
    CHANGED,
    ENABLED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (AgAccountService, ag_account_service, G_TYPE_OBJECT);

#define AG_ACCOUNT_SERVICE_PRIV(obj) (AG_ACCOUNT_SERVICE(obj)->priv)

static gboolean
check_enabled (AgAccountServicePrivate *priv)
{
    gboolean account_enabled;
    gboolean service_enabled;

    ag_account_select_service (priv->account, NULL);
    account_enabled = ag_account_get_enabled (priv->account);

    if (priv->service)
    {
        ag_account_select_service (priv->account, priv->service);
        service_enabled = ag_account_get_enabled (priv->account);
    }
    else
        service_enabled = TRUE;

    return service_enabled && account_enabled;
}

static void
account_watch_cb (AgAccount *account, const gchar *key, gpointer user_data)
{
    AgAccountService *self = (AgAccountService *)user_data;

    g_signal_emit (self, signals[CHANGED], 0);
}

static void
on_account_enabled (AgAccount *account,
                    const gchar *service_name,
                    gboolean service_enabled,
                    AgAccountService *self)
{
    AgAccountServicePrivate *priv = self->priv;
    gboolean enabled;

    DEBUG_INFO ("service: %s, enabled: %d", service_name, service_enabled);

    enabled = check_enabled (priv);
    if (enabled != priv->enabled)
    {
        priv->enabled = enabled;
        g_signal_emit (self, signals[ENABLED], 0, enabled);
    }
}

static void
ag_account_service_set_property (GObject *object, guint property_id,
                                 const GValue *value, GParamSpec *pspec)
{
    AgAccountServicePrivate *priv = AG_ACCOUNT_SERVICE_PRIV (object);

    switch (property_id)
    {
    case PROP_ACCOUNT:
        g_assert (priv->account == NULL);
        priv->account = g_value_dup_object (value);
        break;
    case PROP_SERVICE:
        g_assert (priv->service == NULL);
        priv->service = g_value_dup_boxed (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }

}

static void
ag_account_service_constructed (GObject *object)
{
    AgAccountServicePrivate *priv = AG_ACCOUNT_SERVICE_PRIV (object);

    if (G_UNLIKELY (!priv->account))
    {
        g_warning ("AgAccountService constructed with no account!");
        return;
    }

    priv->account_enabled_id =
        g_signal_connect (priv->account, "enabled",
                          G_CALLBACK (on_account_enabled), object);

    ag_account_select_service (priv->account, priv->service);
    priv->watch = ag_account_watch_dir (priv->account, "",
                                        account_watch_cb, object);

    priv->enabled = check_enabled (priv);
}

static void
ag_account_service_dispose (GObject *object)
{
    AgAccountServicePrivate *priv = AG_ACCOUNT_SERVICE_PRIV (object);

    if (priv->account)
    {
        ag_account_remove_watch (priv->account, priv->watch);
        g_signal_handler_disconnect (priv->account, priv->account_enabled_id);
        g_object_unref (priv->account);
        priv->account = NULL;
    }

    if (priv->service)
    {
        ag_service_unref (priv->service);
        priv->service = NULL;
    }

    G_OBJECT_CLASS (ag_account_service_parent_class)->dispose (object);
}

static void
ag_account_service_class_init(AgAccountServiceClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (AgAccountServicePrivate));

    object_class->constructed = ag_account_service_constructed;
    object_class->dispose = ag_account_service_dispose;
    object_class->set_property = ag_account_service_set_property;

    g_object_class_install_property
        (object_class, PROP_ACCOUNT,
         g_param_spec_object ("account", "account", "account",
                              AG_TYPE_ACCOUNT,
                              G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property
        (object_class, PROP_SERVICE,
         g_param_spec_boxed ("service", "service", "service",
                             ag_service_get_type(),
                             G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

    signals[CHANGED] = g_signal_new ("changed",
        G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST,
        0,
        NULL, NULL,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);

    signals[ENABLED] = g_signal_new ("enabled",
        G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST,
        0,
        NULL, NULL,
        g_cclosure_marshal_VOID__BOOLEAN,
        G_TYPE_NONE,
        1, G_TYPE_BOOLEAN);
}

static void
ag_account_service_init(AgAccountService *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, AG_TYPE_ACCOUNT_SERVICE,
                                              AgAccountServicePrivate);
}

AgAccountService *
ag_account_service_new(AgAccount *account, AgService *service)
{
    g_return_val_if_fail (AG_IS_ACCOUNT (account), NULL);

    return g_object_new (AG_TYPE_ACCOUNT_SERVICE,
                         "account", account,
                         "service", service,
                         NULL);
}

AgAccount *
ag_account_service_get_account (AgAccountService *self)
{
    g_return_val_if_fail (AG_IS_ACCOUNT_SERVICE (self), NULL);

    return self->priv->account;
}

AgService *
ag_account_service_get_service (AgAccountService *self)
{
    g_return_val_if_fail (AG_IS_ACCOUNT_SERVICE (self), NULL);

    return self->priv->service;
}

gboolean
ag_account_service_get_enabled (AgAccountService *self)
{
    g_return_val_if_fail (AG_IS_ACCOUNT_SERVICE (self), FALSE);

    return self->priv->enabled;
}

AgSettingSource
ag_account_service_get_value (AgAccountService *self, const gchar *key,
                              GValue *value)
{
    AgAccountServicePrivate *priv;

    g_return_val_if_fail (AG_IS_ACCOUNT_SERVICE (self), AG_SETTING_SOURCE_NONE);
    priv = self->priv;

    ag_account_select_service (priv->account, priv->service);
    return ag_account_get_value (priv->account, key, value);
}

void
ag_account_service_set_value (AgAccountService *self, const gchar *key,
                              const GValue *value)
{
    AgAccountServicePrivate *priv;

    g_return_if_fail (AG_IS_ACCOUNT_SERVICE (self));
    priv = self->priv;

    ag_account_select_service (priv->account, priv->service);
    ag_account_set_value (priv->account, key, value);
}

void
ag_account_service_settings_iter_init (AgAccountService *self,
                                       AgAccountSettingIter *iter,
                                       const gchar *key_prefix)
{
    AgAccountServicePrivate *priv;

    g_return_if_fail (AG_IS_ACCOUNT_SERVICE (self));
    priv = self->priv;

    ag_account_select_service (priv->account, priv->service);
    ag_account_settings_iter_init (priv->account, iter, key_prefix);
}

gboolean
ag_account_service_settings_iter_next (AgAccountSettingIter *iter,
                                       const gchar **key,
                                       const GValue **value)
{
    return ag_account_settings_iter_next (iter, key, value);
}

gchar **
ag_account_service_get_changed_fields (AgAccountService *self)
{
    AgAccountServicePrivate *priv;
    GHashTable *settings;
    GList *keys, *list;
    gchar **fields;
    gint i;

    g_return_val_if_fail (AG_IS_ACCOUNT_SERVICE (self), NULL);
    priv = self->priv;

    settings = _ag_account_get_service_changes (priv->account, priv->service);
    keys = g_hash_table_get_keys (settings);
    fields = g_malloc ((g_hash_table_size (settings) + 1) *
                       sizeof(gchar *));

    i = 0;
    for (list = keys; list != NULL; list = list->next)
    {
        fields[i++] = g_strdup ((gchar *)(list->data));
    }
    fields[i] = NULL;
    g_list_free (keys);

    return fields;
}

