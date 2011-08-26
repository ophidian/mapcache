/*
 *  Copyright 2010 Thomas Bonfort
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include "geocache.h"
#include <apr_strings.h>
#include <math.h>

/** \addtogroup services */
/** @{ */

static char *demo_head = 
      "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n"
      "  <head>\n"
      "    <style type=\"text/css\">\n"
      "    #map {\n"
      "    width: 100%;\n"
      "    height: 100%;\n"
      "    border: 1px solid black;\n"
      "    }\n"
      "    </style>\n"
      "    <script src=\"http://www.openlayers.org/api/OpenLayers.js\"></script>\n"
      "    <script type=\"text/javascript\">\n"
      "var map;\n"
      "function init(){\n"
      "    map = new OpenLayers.Map( 'map' );\n";

static char *demo_layer =
      "    var %s_%s_layer = new OpenLayers.Layer.WMS( \"%s-%s\",\n"
      "        \"%s\",{layers: '%s'},\n"
      "        { gutter:0,buffer:0,isBaseLayer:true,transitionEffect:'resize',\n"
      "          resolutions:[%s],\n"
      "          units:\"%s\",\n"
      "          maxExtent: new OpenLayers.Bounds(%f,%f,%f,%f),\n"
      "          projection: new OpenLayers.Projection(\"%s\")\n"
      "        }\n"
      "    );\n";  

static char *demo_layer_singletile =
      "    var %s_%s_slayer = new OpenLayers.Layer.WMS( \"%s-%s (singleTile)\",\n"
      "        \"%s\",{layers: '%s'},\n"
      "        { gutter:0,ratio:1,isBaseLayer:true,transitionEffect:'resize',\n"
      "          resolutions:[%s],\n"
      "          units:\"%s\",\n"
      "          singleTile:true,\n"
      "          maxExtent: new OpenLayers.Bounds(%f,%f,%f,%f),\n"
      "          projection: new OpenLayers.Projection(\"%s\")\n"
      "        }\n"
      "    );\n";  

/**
 * \brief parse a demo request
 * \private \memberof geocache_service_demo
 * \sa geocache_service::parse_request()
 */
void _geocache_service_demo_parse_request(geocache_context *ctx, geocache_service *this, geocache_request **request,
      const char *cpathinfo, apr_table_t *params, geocache_cfg *config) {
   geocache_request_get_capabilities_demo *drequest =
      (geocache_request_get_capabilities_demo*)apr_pcalloc(
            ctx->pool,sizeof(geocache_request_get_capabilities_demo));
   *request = (geocache_request*)drequest;
   (*request)->type = GEOCACHE_REQUEST_GET_CAPABILITIES;
   if(!cpathinfo || *cpathinfo=='\0' || !strcmp(cpathinfo,"/")) {
      /*we have no specified service, create the link page*/
      drequest->service = NULL;
      return;
   } else {
      cpathinfo++; /* skip the leading / */
      int i;
      for(i=0;i<GEOCACHE_SERVICES_COUNT;i++) {
         /* loop through the services that have been configured */
         int prefixlen;
         geocache_service *service = NULL;
         service = config->services[i];
         if(!service) continue; /* skip an unconfigured service */
         prefixlen = strlen(service->url_prefix);
         if(strncmp(service->url_prefix,cpathinfo, prefixlen)) continue; /*skip a service who's prefix does not correspond */
         if(*(cpathinfo+prefixlen)!='/' && *(cpathinfo+prefixlen)!='\0') continue; /*we matched the prefix but there are trailing characters*/
         drequest->service = service;
         return;
      }
      ctx->set_error(ctx,404,"demo service \"%s\" not recognised or not enabled",cpathinfo);
   }
}

void _create_demo_front(geocache_context *ctx, geocache_request_get_capabilities *req,
      const char *urlprefix) {
   req->mime_type = apr_pstrdup(ctx->pool,"text/html");
   char *caps = apr_pstrdup(ctx->pool,
         "<html><head><title>geocache demo landing page</title></head><body>");
   int i;
   for(i=0;i<GEOCACHE_SERVICES_COUNT;i++) {
      geocache_service *service = ctx->config->services[i];
      if(!service || service->type == GEOCACHE_SERVICE_DEMO) continue; /* skip an unconfigured service, and the demo one */
      caps = apr_pstrcat(ctx->pool,caps,"<a href=\"",urlprefix,"/demo/",service->url_prefix,"\">",
            service->url_prefix,"</a><br/>",NULL);
   }

   req->capabilities = caps;
}

void _create_demo_wms(geocache_context *ctx, geocache_request_get_capabilities *req,
         const char *url_prefix) {
   req->mime_type = apr_pstrdup(ctx->pool,"text/html");
   char *caps = apr_pstrdup(ctx->pool,demo_head);
   apr_hash_index_t *tileindex_index = apr_hash_first(ctx->pool,ctx->config->tilesets);
   char *layers="";
   while(tileindex_index) {
      geocache_tileset *tileset;
      const void *key; apr_ssize_t keylen;
      apr_hash_this(tileindex_index,&key,&keylen,(void**)&tileset);
      int i,j;
      for(j=0;j<tileset->grid_links->nelts;j++) {
         char *resolutions="";
         char *unit="dd";
         geocache_grid *grid = APR_ARRAY_IDX(tileset->grid_links,j,geocache_grid_link*)->grid;
         if(grid->unit == GEOCACHE_UNIT_METERS) {
            unit="m";
         } else if(grid->unit == GEOCACHE_UNIT_FEET) {
            unit="ft";
         }
         layers = apr_psprintf(ctx->pool,"%s,%s_%s_layer",layers,tileset->name,grid->name);
         resolutions = apr_psprintf(ctx->pool,"%s%.20f",resolutions,grid->levels[0]->resolution);
         
         for(i=1;i<grid->nlevels;i++) {
            resolutions = apr_psprintf(ctx->pool,"%s,%.20f",resolutions,grid->levels[i]->resolution);
         }
         char *ol_layer = apr_psprintf(ctx->pool,demo_layer,
               tileset->name,
               grid->name,
               tileset->name,
               grid->name,
               apr_pstrcat(ctx->pool,url_prefix,"/wms?",NULL),
               tileset->name,resolutions,unit,
               grid->extent[0],
               grid->extent[1],
               grid->extent[2],
               grid->extent[3],
               grid->srs);
         caps = apr_psprintf(ctx->pool,"%s%s",caps,ol_layer);

#ifdef USE_CAIRO
         layers = apr_psprintf(ctx->pool,"%s,%s_%s_slayer",layers,tileset->name,grid->name);
         ol_layer = apr_psprintf(ctx->pool,demo_layer_singletile,
               tileset->name,
               grid->name,
               tileset->name,
               grid->name,
               apr_pstrcat(ctx->pool,url_prefix,"/wms?",NULL),
               tileset->name,resolutions,unit,
               grid->extent[0],
               grid->extent[1],
               grid->extent[2],
               grid->extent[3],
               grid->srs);
         caps = apr_psprintf(ctx->pool,"%s%s",caps,ol_layer);
#endif
      }
      tileindex_index = apr_hash_next(tileindex_index);
   }
   /*skip leading comma */
   layers++;
   caps = apr_psprintf(ctx->pool,"%s"
         "    map.addLayers([%s]);\n"
               "    if(!map.getCenter())\n"
               "     map.zoomToMaxExtent();\n"
               "    map.addControl(new OpenLayers.Control.LayerSwitcher());\n"
               "    map.addControl(new OpenLayers.Control.MousePosition());\n"
               "}\n"
               "    </script>\n"
               "  </head>\n"
               "\n"
               "<body onload=\"init()\">\n"
               "    <div id=\"map\">\n"
               "    </div>\n"
               "</body>\n"
               "</html>\n",caps,layers);
   
   req->capabilities = caps;
}

void _create_capabilities_demo(geocache_context *ctx, geocache_request_get_capabilities *req,
      char *url, char *path_info, geocache_cfg *cfg) {
   geocache_request_get_capabilities_demo *request = (geocache_request_get_capabilities_demo*)req;
   const char *onlineresource = apr_table_get(cfg->metadata,"url");
   if(!onlineresource) {
      onlineresource = url;
   }

   if(!request->service) {
      return _create_demo_front(ctx,req,onlineresource);
   } else {
      switch(request->service->type) {
         case GEOCACHE_SERVICE_WMS:
            return _create_demo_wms(ctx,req,onlineresource);
         case GEOCACHE_SERVICE_GMAPS:
         case GEOCACHE_SERVICE_TMS:
         case GEOCACHE_SERVICE_WMTS:
         case GEOCACHE_SERVICE_KML:
         case GEOCACHE_SERVICE_VE:
            req->mime_type = apr_pstrdup(ctx->pool,"text/plain");
            req->capabilities = apr_pstrdup(ctx->pool,"not implemented");
            return;
         case GEOCACHE_SERVICE_DEMO:
            ctx->set_error(ctx,400,"selected service does not provide a demo page");
            return;
      }
   }


   
}

geocache_service* geocache_service_demo_create(geocache_context *ctx) {
   geocache_service_demo* service = (geocache_service_demo*)apr_pcalloc(ctx->pool, sizeof(geocache_service_demo));
   if(!service) {
      ctx->set_error(ctx, 500, "failed to allocate demo service");
      return NULL;
   }
   service->service.url_prefix = apr_pstrdup(ctx->pool,"demo");
   service->service.type = GEOCACHE_SERVICE_DEMO;
   service->service.parse_request = _geocache_service_demo_parse_request;
   service->service.create_capabilities_response = _create_capabilities_demo;
   return (geocache_service*)service;
}

/** @} *//* vim: ai ts=3 sts=3 et sw=3
*/
