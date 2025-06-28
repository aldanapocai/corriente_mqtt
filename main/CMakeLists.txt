idf_component_register(
    SRCS          "app_main.c"
    INCLUDE_DIRS  "."
    EMBED_TXTFILES hivemq_root_ca.pem
)
