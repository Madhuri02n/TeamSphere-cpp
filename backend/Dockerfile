# Builds and runs the C++ backend. Use this on Render (Runtime: Docker, Root Directory: backend)
# and on Windows with Docker Desktop.
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential pkg-config libmongoc-dev libbson-dev ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .
RUN make teamsphere

ENV PORT=5000
EXPOSE 5000
CMD ["./teamsphere"]
