#include "contiki.h"
#include "sys/node-id.h"
#include "sys/log.h"
#include "net/ipv6/uip-ds6-route.h"
#include "net/ipv6/uip-sr.h"
#include "net/mac/tsch/tsch.h"
#include "net/routing/routing.h"
#include "dev/leds.h"
#define DEBUG DEBUG_PRINT
#include "net/ipv6/uip-debug.h"

#include "sys/log.h"
     #define LOG_MODULE "App"
     #define LOG_LEVEL LOG_LEVEL_DBG

/*---------------------------------------------------------------------------*/
PROCESS(node_process, "RPL Node");
AUTOSTART_PROCESSES(&node_process);

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(node_process, ev, data)
{
  int is_coordinator;

  PROCESS_BEGIN();

  is_coordinator = 0;

  is_coordinator = (node_id == 1);

  if(is_coordinator) {
    NETSTACK_ROUTING.root_start();
    LOG_INFO("Root node started RPL process.\n");
  }
  NETSTACK_MAC.on();


  {
    static struct etimer et;
    /* Print out routing tables every minute */
    etimer_set(&et, CLOCK_SECOND * 10);
    while(1) {
        LOG_INFO("Routing entries: %u\n", uip_ds6_route_num_routes());

        if(uip_ds6_route_num_routes() > 0){
          // Intermediate or root
          if(node_id == 1)
          {
            // ROOT node
            printf("I AM ROOT (%i)!\n", node_id);
            leds_on(LEDS_GREEN);
            leds_off(LEDS_YELLOW);
            leds_off(LEDS_RED);

          }

          else{
            // Intermediate Node
            printf("INTERMEDIATE NODE (%i)!\n", node_id);
            leds_on(LEDS_YELLOW);
            leds_off(LEDS_GREEN);
            leds_off(LEDS_RED);
          }
        }
        else {
          // Node is either leaf or not in network

          // If reachable --> leafnode
          if(NETSTACK_ROUTING.node_is_reachable() && node_id != 1)
          {
            leds_on(LEDS_RED);
            leds_off(LEDS_GREEN);
            leds_off(LEDS_YELLOW);
          }
          else
          {
            leds_off(LEDS_RED);
            leds_off(LEDS_GREEN);
            leds_off(LEDS_YELLOW);
          }
        }
        

        uip_ds6_route_t *route = uip_ds6_route_head();  
        while(route) 
        {
          LOG_INFO("Route ");
          LOG_INFO_6ADDR(&route->ipaddr);
          LOG_INFO_(" via ");
          LOG_INFO_6ADDR(uip_ds6_route_nexthop(route));
          LOG_INFO_("\n");
          route = uip_ds6_route_next(route);
	      }

      	PROCESS_YIELD_UNTIL(etimer_expired(&et));
      	etimer_reset(&et);
    }
  }

  PROCESS_END();
}

