/*
 * Placeholder for libxml/libxml2 libraries, use this header instead.
 *
 * $Id: setup-xml.h,v 1.3 2006-03-08 18:59:35 megastep Exp $ 
 */

#ifndef __SETUP_XML_H__
#define __SETUP_XML_H__

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlmemory.h>

#if LIBXML_VERSION < 20000
/* libxml1 */

#define XML_ROOT(doc) ((doc)->root)
#define XML_CHILDREN(node) ((node)->childs)
#define XML_ADD_TEXT(parent, text)

/* Useful if using Glade */
#define GLADE_XML_UNREF(glade) gtk_object_unref(GTK_OBJECT(glade))
#define GLADE_XML_NEW(a, b) glade_xml_new(a, b)

int xmlUnsetProp(xmlNodePtr node, const xmlChar *name);

#else
/* libxml2 */

#define XML_ROOT(doc) xmlDocGetRootElement(doc)
#define XML_CHILDREN(node) ((node)->children)
#define XML_ADD_TEXT(parent, text) xmlAddChild((parent),xmlNewText(text))

#define GLADE_XML_UNREF(glade) g_object_unref(G_OBJECT(glade))
#define GLADE_XML_NEW(a, b) glade_xml_new(a, b, NULL)

#endif

#endif
