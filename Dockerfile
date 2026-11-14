# Reproducible Linux build + headless smoke test of the offline core.
# docker build -t aiche-health . && docker run --rm aiche-health
FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y build-essential cmake pkg-config \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev curl && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j"$(nproc)"
# the UI needs a display; in the container we verify the offline core only
CMD ["bash", "-c", "./build/aiche-core --data build/data --profiles profiles --profile demo_expedition.json & sleep 5 && curl -s http://127.0.0.1:8787/api/state | head -c 400"]

