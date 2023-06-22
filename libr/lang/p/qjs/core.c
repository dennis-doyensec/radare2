#if 0

# QuickJS core plugin example

```js
(function() {
	let { log } = console;

	function examplePlugin() {
		function coreCall(input) {
			if (input.startsWith("t1")) {
				log("This is a QJS test");
				return true;
			}
			return false;
		}
		return {
			name: "qjs-example",
			desc: "Example QJS plugin (type 't1') in the r2 shell",
			call: coreCall,
		};
	}

	function examplePlugin2() {
		function coreCall(input) {
			if (input.startsWith("t2")) {
				log("This is another QJS test");
				return true;
			}
			return false;
		}
		return {
			name: "qjs-example2",
			desc: "Example QJS plugin (type 't2') in the r2 shell",
			call: coreCall,
		};
	}

	log("Installing the `qjs-example` core plugin");
	log("Type 'test' to confirm it works");
	console.log("load true", r2.plugin("core", examplePlugin));
	console.log("load true", r2.plugin("core", examplePlugin2));
	if (false) {
		console.log("load true", r2.plugin("core", examplePlugin));
		console.log("load true", r2.plugin("core", examplePlugin2));
		console.log("load false", r2.plugin("core", examplePlugin));
		console.log("unload false", r2.unload("false"));
		console.log("unload true", r2.unload("qjs-example"));
		console.log("unload false", r2.unload("qjs-example"));
		log("Plugins:");
		log(r2cmd("Lc"));
	}
})();
```

#endif

// TODO maybe add a function to call by plugin name? (is 1 extra arg)
static int r2plugin_core_call(void *c, const char *input) {
	RCore *core = c;
	QjsPluginData *pd = R_UNWRAP4 (core, lang, session, plugin_data);

	// Iterate over plugins until one returns "true" (meaning the plugin handled the input)
	QjsCorePlugin *plugin;
	R_VEC_FOREACH (&pd->pm.core_plugins, plugin) {
		QjsContext *qc = &plugin->qctx;
		JSValueConst args[1] = { JS_NewString (qc->ctx, input) };
		JSValue res = JS_Call (qc->ctx, qc->func, JS_UNDEFINED, countof (args), args);
		if (JS_ToBool (qc->ctx, res)) {
			return true;
		}
	}

	return false;
}

static JSValue r2plugin_core(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	JSRuntime *rt = JS_GetRuntime (ctx);
	QjsPluginData *pd = JS_GetRuntimeOpaque (rt);
	QjsContext *k = &pd->qc;
	RCore *core = k->core;

	if (argc != 2) {
		return JS_ThrowRangeError (ctx, "r2.plugin expects two arguments");
	}

	JSValueConst args[1] = { JS_NewString (ctx, ""), };

	// check if res is an object
	JSValue res = JS_Call (ctx, argv[1], JS_UNDEFINED, countof (args), args);
	if (!JS_IsObject (res)) {
		return JS_ThrowRangeError (ctx, "r2.plugin function must return an object");
	}

	RCorePlugin *ap = R_NEW0 (RCorePlugin);
	if (!ap) {
		return JS_ThrowRangeError (ctx, "heap stuff");
	}

	JSValue name = JS_GetPropertyStr (ctx, res, "name");
	size_t namelen;
	const char *nameptr = JS_ToCStringLen2 (ctx, &namelen, name, false);
	if (nameptr) {
		ap->name = nameptr;
	} else {
		R_LOG_WARN ("r2.plugin requires the function to return an object with the `name` field");
		return JS_NewBool (ctx, false);
	}

	JSValue desc = JS_GetPropertyStr (ctx, res, "desc");
	const char *descptr = JS_ToCStringLen2 (ctx, &namelen, desc, false);
	if (descptr) {
		ap->desc = strdup (descptr);
	}

	JSValue license = JS_GetPropertyStr (ctx, res, "license");
	const char *licenseptr = JS_ToCStringLen2 (ctx, &namelen, license, false);
	if (licenseptr) {
		ap->license = strdup (licenseptr);
	}

	JSValue func = JS_GetPropertyStr (ctx, res, "call");
	if (!JS_IsFunction (ctx, func)) {
		R_LOG_WARN ("r2.plugin requires the function to return an object with the `call` field to be a function");
		// return JS_ThrowRangeError (ctx, "r2.plugin requires the function to return an object with the `call` field to be a function");
		return JS_NewBool (ctx, false);
	}

	QjsPluginManager *pm = &pd->pm;
	QjsCorePlugin *cp = plugin_manager_find_core_plugin (pm, core, ap->name);
	if (cp) {
		R_LOG_WARN ("r2.plugin with name %s is already registered", ap->name);
		free (ap);
		// return JS_ThrowRangeError (ctx, "r2.plugin core already registered (only one exists)");
		return JS_NewBool (ctx, false);
	}

	/* QjsCorePlugin *plugin = */ plugin_manager_add_core_plugin (pm, core, nameptr, ctx, func);

	// XXX split this function into 2 parts, plugin adding should be separate from running
	ap->call = r2plugin_core_call;

	int ret = -1;
	RLibStruct *lib = R_NEW0 (RLibStruct);
	if (lib) {
		lib->type = R_LIB_TYPE_CORE;
		lib->data = ap;
		lib->version = R2_VERSION;
		ret = r_lib_open_ptr (core->lib, nameptr, NULL, lib);
	}
	return JS_NewBool (ctx, ret == 0);
}
