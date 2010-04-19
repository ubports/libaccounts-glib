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

#ifndef _AG_MANAGER_H_
#define _AG_MANAGER_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define AG_TYPE_MANAGER             (ag_manager_get_type ())
#define AG_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), AG_TYPE_MANAGER, AgManager))
#define AG_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), AG_TYPE_MANAGER, AgManagerClass))
#define AG_IS_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), AG_TYPE_MANAGER))
#define AG_IS_MANAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), AG_TYPE_MANAGER))
#define AG_MANAGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), AG_TYPE_MANAGER, AgManagerClass))

typedef struct _AgManagerClass AgManagerClass;
typedef struct _AgManagerPrivate AgManagerPrivate;
typedef struct _AgManager AgManager;

typedef guint AgAccountId;

#include <libaccounts-glib/ag-provider.h>
#include <libaccounts-glib/ag-service.h>
#include <libaccounts-glib/ag-account.h>

struct _AgManagerClass
{
    GObjectClass parent_class;
    void (*account_deleted) (AgManager *manager, AgAccountId id);
    void (*_ag_reserved2) (void);
    void (*_ag_reserved3) (void);
    void (*_ag_reserved4) (void);
    void (*_ag_reserved5) (void);
    void (*_ag_reserved6) (void);
    void (*_ag_reserved7) (void);
};

struct _AgManager
{
    GObject parent_instance;

    /*< private >*/
    AgManagerPrivate *priv;
};

GType ag_manager_get_type (void) G_GNUC_CONST;

AgManager *ag_manager_new (void);

AgManager *ag_manager_new_for_service_type (const gchar *service_type);

GList *ag_manager_list (AgManager *manager);
GList *ag_manager_list_by_service_type (AgManager *manager,
                                        const gchar *service_type);
void ag_manager_list_free (GList *list);

AgAccount *ag_manager_get_account (AgManager *manager,
                                   AgAccountId account_id);
AgAccount *ag_manager_create_account (AgManager *manager,
                                      const gchar *provider_name);

AgService *ag_manager_get_service (AgManager *manager,
                                   const gchar *service_name);
GList *ag_manager_list_services (AgManager *manager);
GList *ag_manager_list_services_by_type (AgManager *manager,
                                         const gchar *service_type);
GList *ag_manager_list_enabled (AgManager *manager);
GList *ag_manager_list_enabled_by_service_type (AgManager *manager,
                                                const gchar *service_type);
const gchar *ag_manager_get_service_type (AgManager *manager);

AgProvider *ag_manager_get_provider (AgManager *manager,
                                     const gchar *provider_name);
GList *ag_manager_list_providers (AgManager *manager);

G_END_DECLS

#endif /* _AG_MANAGER_H_ */
