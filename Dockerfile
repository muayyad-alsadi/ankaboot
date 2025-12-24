FROM alpine:latest AS builder

# Install build dependencies (make and gcc)
RUN apk add --no-cache make gcc libc-dev

# Set the working directory inside the container
WORKDIR /app

# Copy the source code and the Makefile into the container
COPY *.c Makefile ./

# Run the 'make' command to compile the program
RUN make

# Stage 2: Run the executable (optional, for a smaller final image)
# Use a minimal runtime image
FROM alpine:latest AS runner

# Set the working directory
WORKDIR /app

# Copy the compiled executable from the 'builder' stage
COPY --from=builder /app/ankaboot ./ankaboot
COPY public ./public

# Command to run the executable when the container starts
CMD ["./ankaboot", "-d", "./public", "-p", "8080"]
# Expose the port the application will run on
EXPOSE 8080

