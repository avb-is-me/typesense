FROM typesense/typesense:0.21.0
ENV TYPESENSE_DATA_DIR “/tmp/ts”
ENV TYPESENSE_API_KEY “”
ENV TYPESENSE_API_PORT “8080”
EXPOSE 8080
CMD ["–enable-cors"]
RUN mkdir -p /tmp/ts
