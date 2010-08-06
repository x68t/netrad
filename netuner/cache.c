#include <string.h>
#include <stdlib.h>
#include <libmemcached/memcached.h>
#include <libmemcached/memcached.h>
#include <libxml/tree.h>
#include <libxml/parser.h>

#define SERVER "localhost"
#define PORT 11211
#define EXPIRE 3600

xmlDocPtr cache_open(const char *URL)
{
    memcached_st *mc;
    memcached_return result;
    xmlDocPtr doc;
    int size;
    uint32_t flags;
    char *xml;
    size_t len;

    if (!(mc = memcached_create(NULL)))
        return NULL;

    doc = NULL;
    if (memcached_server_add(mc, SERVER, PORT) != MEMCACHED_SUCCESS)
        goto out;

    xml = memcached_get(mc, URL, strlen(URL), &len, &flags, &result);
    if (result == MEMCACHED_SUCCESS) {
        doc = xmlReadMemory(xml, len, URL, NULL, 0);
        free(xml);
    } else {
        if (!(doc = xmlReadFile(URL, NULL, 0)))
            goto out;
        xmlDocDumpMemory(doc, &xml, &size);
        len = size;
        memcached_set(mc, URL, strlen(URL), xml, len, EXPIRE, 0);
        free(xml);
    }

  out:
    memcached_free(mc);

    return doc;
}
