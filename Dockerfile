# Stage 1: Build AnuDB
FROM ubuntu:22.04 as builder

# Install required packages
RUN apt-get update && apt-get install -y cmake git libzstd-dev mosquitto mosquitto-clients build-essential cmake && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Clone AnuDB repository
RUN git clone https://github.com/hash-anu/AnuDB.git .

# Initialize submodules
RUN git submodule update --init --recursive

RUN rm -rf build
# Create build directory
RUN mkdir build

# Build AnuDB
WORKDIR /app/build
RUN cmake -DGEN_FILES=OFF -DMBEDTLS_BUILD_TESTING=OFF -DENABLE_TESTING=OFF -DZSTD_INCLUDE_DIR=/user/include -DZSTD_LIB_DIR=/usr/lib/x86_64-linux-gnu/ ..
RUN make -j$(nproc)

# Stage 2: Create a minimal runtime image
FROM ubuntu:22.04

# Set working directory
WORKDIR /data

# Copy the AnuDB binary from the builder stage
COPY --from=builder /app/build/mqtt/AnuDBMqttBridge /usr/local/bin/AnuDBMqttBridge

# Create a volume for data storage
VOLUME ["/data"]

# Set the entrypoint to the AnuDB binary
ENTRYPOINT ["AnuDBMqttBridge"]
CMD []
