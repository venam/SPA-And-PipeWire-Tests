#include <stdio.h>
#include <errno.h>

#include <spa/pod/pod.h>
#include <spa/pod/builder.h>
#include <spa/pod/parser.h>
#include <spa/pod/iter.h>
#include <spa/param/props.h>

/*
 * Format LTV, length(32 bits) type(32 bits) value
 * Always aligned to 8 bytes.
 * Each POD has a 32 bits size field, followed by a 32 bits type field. The size
 * field specifies the size following the type field.

 * Use SPA type system
 * Flat type: None Bool, Id Int, String... or containers like Array, Struct,
 * Object, Sequence, or pointer, file descriptor, choice, or another pod.
 *
 * Each POD is aligned to an 8 byte boundary.
 *
 * See Also, PipeWire project doc/spa-pod.dox
 * https://gitlab.freedesktop.org/pipewire/pipewire/-/blob/master/doc/spa-pod.dox
 */

int
main(int argc, char* argv[])
{
	// A buffer on the stack as a storage, but it could be anywhere
	uint8_t buffer[4096];

	struct spa_pod_builder b;
	// This initialized a pod zone on the stack inside buffer.
	// We can write data in it.
	spa_pod_builder_init(&b, buffer, sizeof(buffer));

	// the frame is a pointer to the current structure
	// it has a start and an end to determine the scope
	struct spa_pod_frame f, f2;

	// let's define a struct with 2 fields
	// if we don't close the frame it means the struct will contain
	// the next value
	// start first frame here
	spa_pod_builder_push_struct(&b, &f);
	spa_pod_builder_int(&b, 5);
	spa_pod_builder_float(&b, 3.1415f);
	/*
	 * Another way to do the same thing:
	 * pod = spa_pod_builder_add_struct(&b,
	 *	SPA_POD_Int(5),
	 *	SPA_POD_Float(3.1415f));
	 */

	// close the frame to say we're done, nothing else in that struct
	// it also returns a pointer to the object we completed
	struct spa_pod *pod;
	pod = spa_pod_builder_pop(&b, &f);

	// another new frame with an object in it
	spa_pod_builder_push_object(&b, &f2, SPA_TYPE_OBJECT_Props, SPA_PARAM_Props);
	spa_pod_builder_prop(&b, SPA_PROP_device, 0);
	spa_pod_builder_string(&b, "hw:0");
	spa_pod_builder_prop(&b, SPA_PROP_frequency, 0);
	spa_pod_builder_float(&b, 440.0);
	/*
	 * Another way to do the same thing:
	 * pod = spa_pod_builder_add_object(&b,
	 *	SPA_TYPE_OBJECT_Props, SPA_PARAM_Props,
	 *	SPA_PROP_device,    SPA_POD_String("hw:0"),
	 *	SPA_PROP_frequency, SPA_POD_Float(440.0f));
	 *
	 */

	// and close the object frame, returning the object
	struct spa_pod *obj_pod;
	obj_pod = spa_pod_builder_pop(&b, &f2);

	// parsing the pods (unmarshalling)

	bool is_struct = spa_pod_is_struct(pod);
	printf("is struct pod %i, podtype %i, podsize %i\n", is_struct, pod->type, pod->size);
	// we can cast it, we know its type
	struct spa_pod_struct *example_struct = (struct spa_pod_struct*)pod;
	struct spa_pod *entry;
	SPA_POD_STRUCT_FOREACH(example_struct, entry) {
		printf("field type:%d\n", entry->type);
		// two ways to get the value, casting or using spa_pod_get_..
		if (spa_pod_is_int(entry)) {
			int32_t ival;
			spa_pod_get_int(entry, &ival);
			printf("found int, pod_int: %d\n", ival);
		}
		if (spa_pod_is_float(entry)) {
			struct spa_pod_float *pod_float = (struct spa_pod_float*)entry;
			printf("found float, pod_float: %f\n", pod_float->value);
		}
	}

	puts("");

	bool is_obj_pod = spa_pod_is_object(obj_pod);
	printf("is object pod %i, podtype %i\n", is_obj_pod, obj_pod->type);
	// same idea, also cast it
	struct spa_pod_object *obj = (struct spa_pod_object*)obj_pod;
	const struct spa_pod_prop *prop;
	SPA_POD_OBJECT_FOREACH(obj, prop) {
		printf("prop key:%d\n", prop->key);
		printf("prop value type:%d\n", prop->value.type);
	}
	prop = spa_pod_find_prop(obj_pod, NULL, SPA_PROP_device);
	printf("Finding directly: prop key:%d\n", prop->key);
	printf("Finding directly: prop value type:%d\n", prop->value.type);

	puts("");

	// using an spa_pod_parser to parse the struct
	struct spa_pod_parser p;
	spa_pod_parser_pod(&p, pod);

	// it needs a frame to enter the buffer
	struct spa_pod_frame parser_frame;
	spa_pod_parser_push_struct(&p, &parser_frame);

	uint32_t val;
	float fval;
	spa_pod_parser_get_int(&p, &val);
	printf("parser found int with val: %i\n", val);
	spa_pod_parser_get_float(&p, &fval);
	printf("parser found float with val: %f\n", fval);

	// pop the frame
	spa_pod_parser_pop(&p, &parser_frame);

	// or directly extracting everything
	spa_pod_parser_get_struct(&p,
		SPA_POD_Id(&val),
		SPA_POD_Int(&fval));
	printf("parser (2) found int with val: %i\n", val);
	printf("parser (2) found float with val: %f\n", fval);

	// or even faster
	spa_pod_parse_struct(pod,
		SPA_POD_Id(&val),
		SPA_POD_Int(&fval));
	printf("parser (3) found int with val: %i\n", val);
	printf("parser (3) found float with val: %f\n", fval);

	// dumping the buffer content in a file
	FILE *outfile = fopen("buffer.dump", "wb");
	if (outfile) {
		fwrite(buffer, pod->size+4+4, 1, outfile);
	}

	/* Example output (marshalling):

	xxd -e buffer.dump
	00000000: 00000020 0000000e 00000004 00000004   ...............
	00000010: 00000005 00000000 00000004 00000006  ................
	00000020: 40490e56 00000000                    V.I@....

	length = 00000020 = 32
	type   = 0000000e = 14 = SPA_TYPE_Struct
	value  =
		length   = 00000004 = 4
		type     = 00000004 = SPA_POD_Int
		value    = 00000005 = 5
		_padding = 00000000 (always align to 8 bytes)

		length   = 00000004 = 4
		type     = 00000006 = SPA_POD_Float
		value    = 40490e56 = 3.1415
		_padding = 00000000 (always align to 8 bytes)
	 */

	return 0;
}

