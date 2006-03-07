/*
 * Placeholder for libxml/libxml2 libraries, use this header instead.
 *
 * $Id: setup-xml.h,v 1.2 2006-03-07 00:50:16 megastep Exp $ 
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

int xmlUnsetProp(xmlNodePtr node, const xmlChar *name);

#else
/* libxml2 */

#define XML_ROOT(doc) xmlDocGetRootElement(doc)
#define XML_CHILDREN(node) ((node)->children)
#define XML_ADD_TEXT(parent, text) xmlAddChild((parent),xmlNewText(text))

#endif

#endif
