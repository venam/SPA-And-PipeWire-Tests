#include <stdio.h>
#include <math.h>
#include <pipewire/pipewire.h>
#include <spa/debug/pod.h>
#include <spa/debug/dict.h>



struct data {
	struct pw_main_loop *loop;
	struct pw_core *core;
	struct pw_registry *registry;
};


static void
registry_event_global(void *userdata, uint32_t id,
		uint32_t permissions, const char *type, uint32_t version,
		const struct spa_dict *props)
{
	// PipeWire got its own type system based on Spa
	if (strcmp(PW_TYPE_INTERFACE_Node, type) != 0) {
		return;
	}
	//fprintf(stderr, "object: id:%u type:%s/%d\n", id, type, version);
	// it needs to at least have props
	if (!props) {
		return;
	}
	// uncomment to see all props
	//spa_debug_dict(4, props);

	// nodes should have names as prop
	// see /usr/include/spa/utils/dict.h
	// store to print later
	const char *node_name = spa_dict_lookup(props, "node.name");
	const char *class_name = spa_dict_lookup(props, "media.class");

	// we confirmed this is a PipeWire node
	// it follows the PW_TYPE_INTERFACE_Node interface
	// get a proxy to it to be able to send it message
	// see /usr/include/pipewire-0.3/pipewire/core.h
	struct data *data = userdata;
	struct pw_node *node = pw_registry_bind(data->registry,
		id, PW_TYPE_INTERFACE_Node, version, 0);
	// see /usr/include/pipewire/node.h for what we
	// can do with a pw_node
	// what we do here is subscribe to the param event
	// then request the list of params

	// the hook needs to be alive as long as we register for events
	// here we do a single rountrip and remove it afterwards
	struct spa_hook node_listener;

	void node_param (void *object, int seq, uint32_t param_id,
		uint32_t index, uint32_t next,
		const struct spa_pod *param) {

		// to dump all the pod, see test_pod.c
		//spa_debug_pod(2, NULL, param);

		// it contains a pod object
		const struct spa_pod_prop *prop;
		prop = spa_pod_find_prop(param, NULL, SPA_PROP_channelVolumes);
		if (prop) {
			//fprintf(stderr, "prop key:%d\n", prop->key);
			if (spa_pod_is_array(&prop->value)) {
				struct spa_pod_array *arr = (struct spa_pod_array*)&prop->value;

				struct spa_pod *iter;
				printf("id:%u\tname:%s\tclass:%s\t", id, node_name, class_name);
				SPA_POD_ARRAY_FOREACH(arr, iter) {
					// cube root to linear
					printf("\tvolume:%f", cbrt(*(float*)iter));
				}
				puts("");
			}
		}

		// stop the roundtrip, we got our reply
		// TODO, one issue is that we might exit before getting all nodes
		pw_main_loop_quit(data->loop);
	}

	// another thing we could register to
	//void node_info (void *object, const struct pw_node_info *info) {
	//	done++;
	//	pw_main_loop_quit(data->loop);
	//	fprintf(stderr, "id: %u", info->id);
	//	spa_debug_dict(4, props);
	//	for (int i = 0; i < info->n_params; i++) {
	//		fprintf(stderr, "params available id: %u\n", info->params[i]);
	//	}
	//}

	const struct pw_node_events node_events = {
		PW_VERSION_NODE_EVENTS,
		.param = node_param,
		//.info  = node_info,
	};

	// not really needed but...
	spa_zero(node_listener);
	// attach the listener for the enum param events
	pw_node_add_listener(node, &node_listener,
		&node_events, NULL);

	// send the enum params event
	// here we only want the SPA_PARAM_Props and nothing else
	// see spa_param_type in /usr/include/spa/param/param.h
	pw_node_enum_params(node, 0,
		SPA_PARAM_Props, 0, //start index
		1, // max num of params
		NULL // filter
	);

	// run the loop
	// this will block until we call pw_main_loop_quit in the callback
	pw_main_loop_run(data->loop);
	// safely remove the hook once we got the param we wanted
	spa_hook_remove(&node_listener);
}

static const struct pw_registry_events
registry_events = {
	PW_VERSION_REGISTRY_EVENTS,
	.global = registry_event_global,
};

int
main(int argc, char *argv[])
{
	//struct pw_main_loop *loop;
	struct pw_context *context;
	//struct pw_core *core;
	struct spa_hook registry_listener;
	struct data data = { };

	pw_init(&argc, &argv);

	data.loop = pw_main_loop_new(NULL /* properties */);
	context = pw_context_new(pw_main_loop_get_loop(data.loop),
			NULL /* properties */,
			0 /* user_data size */);

	data.core = pw_context_connect(context,
			NULL /* properties */,
			0 /* user_data size */);

	data.registry = pw_core_get_registry(data.core, PW_VERSION_REGISTRY,
			0 /* user_data size */);

	spa_zero(registry_listener);
	pw_registry_add_listener(data.registry, &registry_listener,
			&registry_events, (void *)&data);

	pw_main_loop_run(data.loop);

	// destroy objects in reverse order
	// the registry is a proxy to the remote registry object on the
	// pw server
	pw_proxy_destroy((struct pw_proxy*)data.registry);
	pw_core_disconnect(data.core);
	pw_context_destroy(context);
	pw_main_loop_destroy(data.loop);

	return 0;
}

