# Dockerfile for USB Redirector MITM Relay Server (usbredir Core)
FROM alpine:3.19 AS builder

RUN apk add --no-req build-base cmake gcc g++ linux-headers

WORKDIR /app
COPY . .

RUN cmake -B build && cmake --build build --target usb_server

FROM alpine:3.19

RUN apk add --no-cache libgcc libstdc++

WORKDIR /app
COPY --from=builder /app/build/usb_server /app/usb_server

EXPOSE 32400

ENTRYPOINT ["/app/usb_server"]
