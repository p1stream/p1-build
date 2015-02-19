#ifndef p1stream_mac_preview_h
#define p1stream_mac_preview_h

#include <mach/mach.h>
#include <bootstrap.h>


// The initial request to the service. This message should include a
// null-terminated mixer ID in the body, padded with all zeroes. Additionally,
// a send right must be attached on which the peer would like to receive
// notifications.
static const mach_msg_id_t p1_preview_request_msg_id = 0xDADAD0D0;
struct p1_preview_request_msg_t {
    mach_msg_header_t header;
    char mixer_id[128];
};

// The surface changed. A send right for an IOSurface mach port is attached
// that can be used to get an IOSurfaceRef on the receiving side. There may be
// no port attached, which means the current IOSurface should be released.
static const mach_msg_id_t p1_preview_set_surface_msg_id = 0xDADA0001;
typedef mach_msg_empty_rcv_t p1_preview_set_surface_msg_t;

// An update notification is just an empty message with an ID.
static const mach_msg_id_t p1_preview_updated_msg_id = 0xDADA0002;
typedef mach_msg_empty_rcv_t p1_preview_updated_msg_t;

// Union that can be used to receive messages.
union p1_preview_msg_t {
    mach_msg_header_t header;
    p1_preview_set_surface_msg_t set_surface;
    p1_preview_updated_msg_t updated;
};


// Request a preview port for the given mixer ID. The return value can also be
// a kern_return_t, of which mach_msg_return_t is a superset. Check the value
// against KERN_RETURN_MAX if needed.
static inline mach_msg_return_t p1_request_preview(const char *mixer_id, mach_port_t *out_port)
{
    mach_msg_return_t ret;
    mach_port_t bootstrap_port, service_port;

    // Get the bootstrap special port.
    ret = task_get_bootstrap_port(mach_task_self(), &bootstrap_port);
    if (ret != KERN_SUCCESS)
        return ret;

    // Lookup up the service in the bootstrap.
    ret = bootstrap_look_up(bootstrap_port, "com.p1stream.P1stream.preview", &service_port);
    mach_port_deallocate(mach_task_self(), bootstrap_port);
    if (ret != KERN_SUCCESS)
        return ret;

    // Allocate our receive port.
    ret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, out_port);
    if (ret != KERN_SUCCESS) {
        mach_port_deallocate(mach_task_self(), service_port);
        return ret;
    }

    // Send the request message.
    p1_preview_request_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND);
    msg.header.msgh_size = sizeof(msg);
    msg.header.msgh_remote_port = service_port;
    msg.header.msgh_local_port = *out_port;
    msg.header.msgh_id = p1_preview_request_msg_id;
    strncpy(msg.mixer_id, mixer_id, sizeof(msg.mixer_id) - 1);
    ret = mach_msg(
        &msg.header, MACH_SEND_MSG, sizeof(msg), 0, MACH_PORT_NULL,
        MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL
    );
    mach_port_deallocate(mach_task_self(), service_port);
    if (ret != MACH_MSG_SUCCESS)
        mach_port_destroy(mach_task_self(), *out_port);

    return ret;
}


#endif  // p1stream_mac_preview_h
