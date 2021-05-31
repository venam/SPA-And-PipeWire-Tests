#include <stdio.h>
#include <errno.h>
#include <dlfcn.h>

#include <spa/utils/type.h>
#include <spa/support/plugin.h>
#include <spa/support/log.h>

#define SPA_PLUGIN_PATH "/usr/lib/spa-0.2/"

int
main(int argc, char* argv[])
{
	void *hnd = dlopen(SPA_PLUGIN_PATH"/support/libspa-support.so", RTLD_NOW);

	spa_handle_factory_enum_func_t enum_func;
	enum_func = dlsym(hnd, SPA_HANDLE_FACTORY_ENUM_FUNC_NAME);

	uint32_t i;
	const struct spa_handle_factory *factory = NULL;
	for (i = 0;;) {
		if (enum_func(&factory, &i) <= 0)
			break;
		// check name and version, introspect interfaces,
		// do something with the factory.
		const struct spa_interface_info *info = NULL;
		uint32_t index = 0;
		int interface_available = spa_handle_factory_enum_interface_info(factory, &info, &index);
		printf("spa interface info type: %s, interface_available: %i\n",
			info->type, interface_available);
		if (strcmp(info->type, SPA_TYPE_INTERFACE_Log) == 0) {
			puts("Found the logging interface");

			struct spa_dict *extra_params = NULL;
			size_t size = spa_handle_factory_get_size(factory, extra_params);
			printf("size to allocate %i\n", size);
			struct spa_handle *handle = calloc(1, size);
			spa_handle_factory_init(factory, handle,
				NULL, // info
				NULL, // support
				0     // n_support
			);
			if (handle == NULL) {
				printf("handle is null\n");
			}
			void *iface;
			// SPA_TYPE_INTERFACE_Log
			int interface_exists = spa_handle_get_interface(handle, SPA_TYPE_INTERFACE_Log, &iface);
			printf("exists: %i\n", interface_exists);
			struct spa_log *log = iface;
			spa_log_warn(log, "Hello World!");
			spa_log_info(log, "version: %i", log->iface.version);
			spa_handle_clear(handle);
			break;
		}
		
	}

	dlclose(hnd);
	return 0;
}

