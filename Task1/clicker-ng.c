#include <stdio.h>
#include <string.h>
#include "contiki.h"
#include "dev/button-sensor.h"
#include "dev/leds.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"

struct Event {
    int id;
    clock_time_t time;
    linkaddr_t addr;
};

#define MAX_NUMBER_OF_EVENTS 3
struct Event event_history[MAX_NUMBER_OF_EVENTS];

#define ALARM_TIMEOUT 30  // 10 seconds timeout, CLOCK_SECOND not needed due to time being converted before

PROCESS(clicker_ng_process, "Clicker NG Process");
AUTOSTART_PROCESSES(&clicker_ng_process);

/*---------------------------------------------------------------------------*/
void print_event_structure(struct Event *ev_arr) {
    int i;
    for(i = 0; i < MAX_NUMBER_OF_EVENTS; i++) {
        printf("___ EVENT HISTORY %i ___\n", i);
        printf("Time created(sec): %lu\n", (unsigned long)ev_arr[i].time);
        printf("Addr: %x:%x\n\n", ev_arr[i].addr.u8[0], ev_arr[i].addr.u8[1]);
    }
}

/*---------------------------------------------------------------------------*/
void add_event_to_history(const linkaddr_t *addr, int index){
    if (index >= 0 && index < MAX_NUMBER_OF_EVENTS) {
        event_history[index].addr = *addr;  // Store the address
        event_history[index].time = clock_time() / CLOCK_SECOND;  // Store the time (in seconds)
    }
}

/*---------------------------------------------------------------------------*/

void remove_event_from_history(int index)
{
  event_history[index].id = 0;
  event_history[index].time = 0;
  linkaddr_copy(&event_history[index].addr, &linkaddr_null);
}


/*---------------------------------------------------------------------------*/
int find_oldest_index() {
    
    int oldest_time = event_history[0].time; // sets it to the first index for reference
    int oldest_index = 0; // Start from the first index (we assume there is at least one event)

    int i;
    for (i = 1; i < MAX_NUMBER_OF_EVENTS; i++) { // Start from index 1 since we've already considered index 0
        if (event_history[i].time < oldest_time && event_history[i].time != 0) { // Look for the smallest non-zero time
            oldest_time = event_history[i].time;
            oldest_index = i;
        }
    }

    return oldest_index;
}


/*---------------------------------------------------------------------------*/

/*
Updates the event history and checks if an alarm should be triggered.
Called when a broadcast packet is received, or when the button on
the local node is clicked.
*/
void handle_event(const linkaddr_t *src) {
    // printf("IN HANDLE EVENT!\n");

    int event_added = 0;
    int i;
    for(i = 0; i < MAX_NUMBER_OF_EVENTS; i++) {
        if(linkaddr_cmp(src, &event_history[i].addr) || event_history[i].time == 0) {
            add_event_to_history(src, i);
            // printf("OWN EVENT ADDED TO HISTORY\n");
            event_added = 1;
            break;
        }

        
    }
    
    // If no event was added
    if(!event_added)
    {
        // If history is full & the signal is from a new node
        int oldest_index = find_oldest_index();
        if(oldest_index != -1){
            add_event_to_history(src, oldest_index);
        }
        event_added = 1;
    }


    // Check if 3 events has occured
    int j;
    int count = 0;
    for(j=0; j<MAX_NUMBER_OF_EVENTS;j++){
        if(event_history[j].time != 0){
            count++;
        }
    }

    if(count == 3)
    {
        printf("3 EVENTS ACTIVE --> TURNING ON RED LED\n");
        leds_on(LEDS_RED);
    }

    print_event_structure(event_history);
}


/*---------------------------------------------------------------------------*/

static void recv(const void *data, uint16_t len, const linkaddr_t *src, const linkaddr_t *dest) {
    printf("Received: %s - from %d\n", (char*) data, src->u8[0]);
    // leds_toggle(LEDS_GREEN);

    // printf("handling package event...\n");
    handle_event(src);
}


/*---------------------------------------------------------------------------*/
PROCESS_THREAD(clicker_ng_process, ev, data) {
    static char payload[] = "hej";
    static struct etimer timeout_timer;

    PROCESS_BEGIN();

    /* Initialize NullNet */
    nullnet_buf = (uint8_t *)&payload;
    nullnet_len = sizeof(payload);
    nullnet_set_input_callback(recv);

    // Initialize event_history
    int i;
    for(i = 0; i < MAX_NUMBER_OF_EVENTS; i++) {
        struct Event ev = {0};
        event_history[i] = ev;
    }

    /* Activate the button sensor. */
    SENSORS_ACTIVATE(button_sensor);

    // Set the timer to check for timeouts every second
    etimer_set(&timeout_timer, 1 * CLOCK_SECOND);

    while(1) {
        PROCESS_WAIT_EVENT();

        // Check if the button sensor is pressed
        if (ev == sensors_event && data == &button_sensor) {
            // Button pressed
            handle_event(&linkaddr_node_addr);
            // leds_toggle(LEDS_RED);

            memcpy(nullnet_buf, &payload, sizeof(payload));
            nullnet_len = sizeof(payload);

            /* Send the content of the packet buffer using the broadcast handle. */
            NETSTACK_NETWORK.output(NULL);
        }

        // Check if the timeout timer has expired
        if (etimer_expired(&timeout_timer)) {
            clock_time_t current_time = clock_time() / CLOCK_SECOND;

            // Check for events that have timed out
            int j;
            for(j = 0; j < MAX_NUMBER_OF_EVENTS; j++) {

                // If the event exists and it has timed out
                if (event_history[j].time != 0 && (current_time - event_history[j].time) >= ALARM_TIMEOUT) {
                    printf("WE HAVE TIMEOUT for event %d\n", j);


                    // REMOVE EVENT IN HISTORY
                    remove_event_from_history(j);
                    // printf("Event successfully removed\n");
                    leds_off(LEDS_RED); // Turn off Alarm-LED
                }
            }

            // Reset the timer to check again after 5 seconds
            etimer_reset(&timeout_timer);
        }
    }

    PROCESS_END();
}

// printf("TIME IN EVENT: %lu\n", (unsigned long)(event_history[j].time));
// printf("TIME DIFF: %lu,  TIMEOUT TIME: %i\n", (current_time - event_history[j].time), ALARM_TIMEOUT);
// printf("TIMOUT TIME: %lu\n", ALARM_TIMEOUT);