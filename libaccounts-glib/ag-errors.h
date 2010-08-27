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

#ifndef _AG_ERRORS_H_
#define _AG_ERRORS_H_

#include <glib.h>

G_BEGIN_DECLS

GQuark ag_errors_quark (void);

#define AG_ERRORS   ag_errors_quark ()

typedef enum {
    AG_ERROR_DB,
    AG_ERROR_DISPOSED,
    AG_ERROR_DELETED,
    AG_ERROR_DB_LOCKED,
} AgError;

G_END_DECLS

#endif /* _AG_ERRORS_H_ */
