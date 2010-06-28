/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of libaccounts-glib
 *
 * Copyright (C) 2010 Nokia Corporation.
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

#ifndef _AG_SERVICE_TYPE_H_
#define _AG_SERVICE_TYPE_H_

#include <glib.h>

G_BEGIN_DECLS

typedef struct _AgServiceType AgServiceType;

const gchar *ag_service_type_get_name (AgServiceType *service_type);
const gchar *ag_service_type_get_i18n_domain (AgServiceType *service_type);
const gchar *ag_service_type_get_display_name (AgServiceType *service_type);
const gchar *ag_service_type_get_icon_name (AgServiceType *service_type);
void ag_service_type_get_file_contents (AgServiceType *service_type,
                                        const gchar **contents,
                                        gsize *len);
AgServiceType *ag_service_type_ref (AgServiceType *service_type);
void ag_service_type_unref (AgServiceType *service_type);

G_END_DECLS

#endif /* _AG_SERVICE_TYPE_H_ */
