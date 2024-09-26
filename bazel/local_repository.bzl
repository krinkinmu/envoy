def envoy_new_local_repository(name, build_file, path):
    native.new_local_repository(
        name = name,
        build_file = build_file,
        path = path,
    )
