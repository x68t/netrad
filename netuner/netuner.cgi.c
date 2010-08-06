#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include "cache.h"

#define URL_TEMPLATE "http://pri.kts-af.net/xml/index.xml?sid=09A05772824906D82E3679D21CB1158B&tuning_id=%d"

#define MENU_ROOT "/kb:Kerbango/kb:results/kb:menu_record"
#define STATION_ROOT "/kb:Kerbango/kb:results/kb:station_record"

void register_namespaces(xmlNodePtr node, xmlXPathContextPtr xctx)
{
    xmlNsPtr ns;

    for (ns = node->nsDef; ns; ns = ns->next)
        xmlXPathRegisterNs(xctx, ns->prefix, ns->href);
}

xmlNodePtr get_node_with_xpath(const char *xpath, xmlXPathContextPtr ctx)
{
    xmlXPathObjectPtr xobj;
    xmlNodePtr node;

    if (!(xobj = xmlXPathEval((const xmlChar *)xpath, ctx)))
        return NULL;
    if (xobj->type != XPATH_NODESET || !xobj->nodesetval)
        node = NULL;
    else if (xobj->nodesetval->nodeNr == 1)
        node = xobj->nodesetval->nodeTab[0];
    else
        node = NULL;
    xmlXPathFreeObject(xobj);

    return node;
}

const char *get_content(xmlNodePtr node, const char *default_content)
{
    if (!node || node->type != XML_TEXT_NODE)
        return default_content;

    return (const char *)node->content;
}

const char *get_content_with_xpath(const char *xpath, xmlXPathContextPtr ctx, const char *default_content)
{
    xmlNodePtr node;

    if (!(node = get_node_with_xpath(xpath, ctx)))
        return default_content;

    return get_content(node, default_content);
}

void qprint(const char *s, const char *quotes)
{
    char c;

    while ((c = *s++)) {
        if (strchr(quotes, c))
            printf("%%%02X", c);
        else
            putchar(c);
    }
}

xmlDocPtr url_open(int tuning_id)
{
    char buf[1024];

    snprintf(buf, sizeof(buf), URL_TEMPLATE, tuning_id);
    return cache_open(buf);
}

void process(xmlDocPtr doc, const char *root, void (*printer)(xmlXPathContextPtr, xmlNodePtr))
{
    int i;
    xmlXPathContextPtr xctx;
    xmlXPathObjectPtr xobj;

    if (!(xctx = xmlXPathNewContext(doc)))
        return;
    register_namespaces(xmlDocGetRootElement(doc), xctx);

    xobj = xmlXPathEval((const xmlChar *)root, xctx);
    if (!xobj || xobj->type != XPATH_NODESET || !xobj->nodesetval)
        goto out;

    for (i = 0; i < xobj->nodesetval->nodeNr; i++)
        printer(xctx, xobj->nodesetval->nodeTab[i]);

  out:
    if (xobj)
        xmlXPathFreeObject(xobj);
    xmlXPathFreeContext(xctx);
}

void menu_print(xmlXPathContextPtr xctx, xmlNodePtr node)
{
    int menu_id;
    char *p;
    const char *name, *menu_id_string;

    xctx->node = node;
    if (!(name = get_content_with_xpath("kb:name/text()", xctx, NULL)) ||
        !(menu_id_string = get_content_with_xpath("kb:menu_id/text()", xctx, NULL)))
    {
        return;
    }
    menu_id = strtol(menu_id_string, &p, 10);
    if (*p)
        return;

    printf("<li><a href='netuner.cgi?%d'>", menu_id);
    qprint(name, "<>");
    printf("</a></li>\n");
}

void menu()
{
    xmlDocPtr doc;

    if (!(doc = url_open(-12)))
        return;

    process(doc, MENU_ROOT, menu_print);
}

void station_print(xmlXPathContextPtr xctx, xmlNodePtr node)
{
    int kbps;
    char *p;
    const char *name, *url, *kbps_string;

    xctx->node = node;
    if (!(name = get_content_with_xpath("kb:station/text()", xctx, NULL)) ||
        !(url = get_content_with_xpath("kb:station_url_record/kb:url/text()", xctx, NULL)) ||
        !(kbps_string = get_content_with_xpath("kb:station_url_record/kb:bandwidth_kbps/text()", xctx, NULL)))
    {
        return;
    }
    kbps = strtol(kbps_string, &p, 10);
    if (*p)
        return;

    printf("<li class='station' onclick='javascript:tune(\"");
    qprint(url, " \"%&'+/<>?");
    printf("\");'><div class='kbps'>%d</div><div class='name'>%s</div></li>\n", kbps, name);
}

void station(int tuning_id)
{
    xmlDocPtr doc;

    if (!(doc = url_open(tuning_id)))
        return;

    printf("<ul>");
    process(doc, STATION_ROOT, station_print);
    printf("</ul>\n");
}

int main()
{
    int tuning_id;
    char *p;
    const char *query_string;

    if (!(query_string = getenv("QUERY_STRING")))
        return 0;
    if (!strcmp(query_string, "-12"))
        menu();
    else {
        tuning_id = strtol(query_string, &p, 10);
        if (*p)
            return 0;
        station(tuning_id);
    }

    return 0;
}
