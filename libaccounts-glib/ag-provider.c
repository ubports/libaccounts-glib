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
 * SECTION:ag-provider
 * @title: AgProvider
 * @short_description: A representation of a provider.
 *
 * The #AgProvider structure represents a provider. The structure is not
 * directly exposed to applications, but its fields are accessible via getter
 * methods.
 * The structure is reference counted. One must use ag_provider_unref() when
 * done with it.
 */

#include "config.h"

#include "ag-provider.h"

#include "ag-internals.h"
#include "ag-util.h"
#include <libxml/xmlreader.h>
#include <string.h>

static const gchar suffix[] = ".provider";
#define SUFFIX_LEN (sizeof(suffix) - 1)

static gint
cmp_provider_name (AgProvider *provider, const gchar *provider_name)
{
    const gchar *name;

    name = ag_provider_get_name (provider);
    if (G_UNLIKELY (!name)) return 1;

    return strcmp (name, provider_name);
}

static void
add_providers_from_dir (AgManager *manager, const gchar *dirname,
                       GList **providers)
{
    const gchar *filename;
    AgProvider *provider;
    gchar provider_name[256];
    GDir *dir;

    g_return_if_fail (providers != NULL);
    g_return_if_fail (dirname != NULL);

    dir = g_dir_open (dirname, 0, NULL);
    if (!dir) return;

    while ((filename = g_dir_read_name (dir)) != NULL)
    {
        if (filename[0] == '.')
            continue;

        if (!g_str_has_suffix (filename, suffix))
            continue;

        g_snprintf (provider_name, sizeof (provider_name),
                    "%.*s", strlen (filename) - SUFFIX_LEN, filename);

        /* if there is already a provider with the same name in the list, then
         * we skip this one (we process directories in descending order of
         * priority) */
        if (g_list_find_custom (*providers, provider_name,
                                (GCompareFunc)cmp_provider_name))
            continue;

        provider = ag_manager_get_provider (manager, provider_name);
        if (G_UNLIKELY (!provider)) continue;

        *providers = g_list_prepend (*providers, provider);
    }

    g_dir_close (dir);
}

GList *
_ag_providers_list (AgManager *manager)
{
    const gchar * const *dirs;
    const gchar *env_dirname, *datadir;
    gchar *dirname;
    GList *providers = NULL;

    env_dirname = g_getenv ("AG_PROVIDERS");
    if (env_dirname)
    {
        add_providers_from_dir (manager, env_dirname, &providers);
        /* If the environment variable is set, don't look in other places */
        return providers;
    }

    datadir = g_get_user_data_dir ();
    if (G_LIKELY (datadir))
    {
        dirname = g_build_filename (datadir, PROVIDER_FILES_DIR, NULL);
        add_providers_from_dir (manager, dirname, &providers);
        g_free (dirname);
    }

    dirs = g_get_system_data_dirs ();
    for (datadir = *dirs; datadir != NULL; dirs++, datadir = *dirs)
    {
        dirname = g_build_filename (datadir, PROVIDER_FILES_DIR, NULL);
        add_providers_from_dir (manager, dirname, &providers);
        g_free (dirname);
    }
    return providers;
}

static gboolean
parse_provider (xmlTextReaderPtr reader, AgProvider *provider)
{
    const gchar *name;
    int ret, type;

    if (!provider->name)
    {
        xmlChar *_name = xmlTextReaderGetAttribute (reader,
                                                    (xmlChar *) "id");
        provider->name = g_strdup((const gchar *)_name);
        if (_name) xmlFree(_name);
    }

    ret = xmlTextReaderRead (reader);
    while (ret == 1)
    {
        name = (const gchar *)xmlTextReaderConstName (reader);
        if (G_UNLIKELY (!name)) return FALSE;

        type = xmlTextReaderNodeType (reader);
        if (type == XML_READER_TYPE_END_ELEMENT &&
            strcmp (name, "provider") == 0)
            break;

        if (type == XML_READER_TYPE_ELEMENT)
        {
            gboolean ok;

            if (strcmp (name, "name") == 0 && !provider->display_name)
            {
                ok = _ag_xml_dup_element_data (reader, &provider->display_name);
                /* that's the only thing we are interested of: we can stop the
                 * parsing now. */
                return TRUE;
            }
            else
                ok = TRUE;

            if (G_UNLIKELY (!ok)) return FALSE;
        }

        ret = xmlTextReaderNext (reader);
    }
    return TRUE;
}

static gboolean
read_provider_file (xmlTextReaderPtr reader, AgProvider *provider)
{
    const xmlChar *name;
    int ret;

    ret = xmlTextReaderRead (reader);
    while (ret == 1)
    {
        name = xmlTextReaderConstName (reader);
        if (G_LIKELY (name &&
                      strcmp ((const gchar *)name, "provider") == 0))
        {
            return parse_provider (reader, provider);
        }

        ret = xmlTextReaderNext (reader);
    }
    return FALSE;
}

static gchar *
find_provider_file (const gchar *provider_id)
{
    const gchar * const *dirs;
    const gchar *dirname;
    const gchar *env_dirname;
    gchar *filename, *filepath;

    filename = g_strdup_printf ("%s.provider", provider_id);
    env_dirname = g_getenv ("AG_PROVIDERS");
    if (env_dirname)
    {
        filepath = g_build_filename (env_dirname, filename, NULL);
        if (g_file_test (filepath, G_FILE_TEST_IS_REGULAR))
            goto found;
        g_free (filepath);
    }

    dirname = g_get_user_data_dir ();
    if (G_LIKELY (dirname))
    {
        filepath = g_build_filename (dirname, "accounts/providers",
                                              filename, NULL);
        if (g_file_test (filepath, G_FILE_TEST_IS_REGULAR))
            goto found;
        g_free (filepath);
    }

    dirs = g_get_system_data_dirs ();
    for (dirname = *dirs; dirname != NULL; dirs++, dirname = *dirs)
    {
        filepath = g_build_filename (dirname, "accounts/providers",
                                              filename, NULL);
        if (g_file_test (filepath, G_FILE_TEST_IS_REGULAR))
            goto found;
        g_free (filepath);
    }

    filepath = NULL;
found:
    g_free (filename);
    return filepath;
}

AgProvider *
_ag_provider_new (void)
{
    AgProvider *provider;

    provider = g_slice_new0 (AgProvider);
    provider->ref_count = 1;

    return provider;
}

static gboolean
_ag_provider_load_from_file (AgProvider *provider)
{
    xmlTextReaderPtr reader;
    gchar *filepath;
    gboolean ret;
    GError *error = NULL;
    gsize len;

    g_return_val_if_fail (provider->name != NULL, FALSE);

    DEBUG_REFS ("Loading provider %s", provider->name);
    filepath = find_provider_file (provider->name);
    if (G_UNLIKELY (!filepath)) return FALSE;

    g_file_get_contents (filepath, &provider->file_data,
                         &len, &error);
    if (G_UNLIKELY (error))
    {
        g_warning ("Error reading %s: %s", filepath, error->message);
        g_error_free (error);
        g_free (filepath);
        return FALSE;
    }

    g_free (filepath);

    /* TODO: cache the xmlReader */
    reader = xmlReaderForMemory (provider->file_data, len,
                                 NULL, NULL, 0);
    if (G_UNLIKELY (reader == NULL))
        return FALSE;

    ret = read_provider_file (reader, provider);

    xmlFreeTextReader (reader);
    return ret;
}

AgProvider *
_ag_provider_new_from_file (const gchar *provider_name)
{
    AgProvider *provider;

    provider = _ag_provider_new ();
    provider->name = g_strdup (provider_name);
    if (!_ag_provider_load_from_file (provider))
    {
        ag_provider_unref (provider);
        provider = NULL;
    }

    return provider;
}

/**
 * ag_provider_get_name:
 * @provider: the #AgProvider.
 *
 * Returns: the name of @provider.
 */
const gchar *
ag_provider_get_name (AgProvider *provider)
{
    g_return_val_if_fail (provider != NULL, NULL);
    return provider->name;
}

/**
 * ag_provider_get_display_name:
 * @provider: the #AgProvider.
 *
 * Returns: the display name of @provider.
 */
const gchar *
ag_provider_get_display_name (AgProvider *provider)
{
    g_return_val_if_fail (provider != NULL, NULL);
    return provider->display_name;
}

/**
 * ag_provider_get_file_contents:
 * @provider: the #AgProvider.
 * @contents: location to receive the pointer to the file contents.
 *
 * Gets the contents of the XML provider file.  The buffer returned in @contents
 * should not be modified or freed, and is guaranteed to be valid as long as
 * @provider is referenced.
 * If some error occurs, @contents is set to %NULL.
 */
void
ag_provider_get_file_contents (AgProvider *provider,
                              const gchar **contents)
{
    g_return_if_fail (provider != NULL);
    g_return_if_fail (contents != NULL);

    if (provider->file_data == NULL)
    {
        /* This can happen if the provider was created by the AccountManager by
         * loading the record from the DB.
         * Now we must reload the provider from its XML file.
         */
        if (!_ag_provider_load_from_file (provider))
            g_warning ("Loading provider %s file failed", provider->name);
    }

    *contents = provider->file_data;
}

/**
 * ag_provider_ref:
 * @provider: the #AgProvider.
 *
 * Adds a reference to @provider.
 *
 * Returns: @provider.
 */
AgProvider *
ag_provider_ref (AgProvider *provider)
{
    g_return_val_if_fail (provider != NULL, NULL);
    g_return_val_if_fail (provider->ref_count > 0, NULL);

    DEBUG_REFS ("Referencing provider %s (%d)",
                provider->name, provider->ref_count);
    provider->ref_count++;
    return provider;
}

/**
 * ag_provider_unref:
 * @provider: the #AgProvider.
 *
 * Used to unreference the #AgProvider structure.
 */
void
ag_provider_unref (AgProvider *provider)
{
    g_return_if_fail (provider != NULL);
    g_return_if_fail (provider->ref_count > 0);

    DEBUG_REFS ("Unreferencing provider %s (%d)",
                provider->name, provider->ref_count);
    provider->ref_count--;
    if (provider->ref_count == 0)
    {
        g_free (provider->name);
        g_free (provider->display_name);
        g_free (provider->file_data);
        g_slice_free (AgProvider, provider);
    }
}

/**
 * ag_provider_list_free:
 * @list: a #GList of providers returned by some function of this library.
 *
 * Frees the list @list.
 */
void
ag_provider_list_free (GList *list)
{
    g_list_foreach (list, (GFunc)ag_provider_unref, NULL);
    g_list_free (list);
}

