import WebSocket from 'ws';

const url = 'ws://localhost:8080/';

class PublishClient {
  constructor(channel: string) {
    this.server = new WebSocket(`${url}${channel}/pub`, { timeout: 500, });
    this.server.onopen = this.onOpen.bind(this);
    this.server.onmessage = this.onMessage.bind(this);
  }

  private onOpen() {
    console.log('Connected to server');
  }

  private onMessage(e: WebSocket.MessageEvent) {
    if (this.timeout === undefined) {
      // First message, ping duration.
      this.timeout = parseInt(e.data as string, 16) + 5000;
      console.log("Timeout: " + this.timeout);
      this.server.on('ping', () => this.setupTimeoutTimer());
      this.setupTimeoutTimer();
      return;
    }
  }

  private setupTimeoutTimer() {
    console.log("Timeout Reset");
    if (this.timeoutTimer) {
      clearTimeout(this.timeoutTimer);
    }
    this.timeoutTimer = setTimeout(() => { console.log("Timeout"); this.server.close() }, this.timeout);
  }

  public publish(data: string, cb?: ((err?: Error | undefined) => void)) {
    this.server.send(data, cb);
    this.setupTimeoutTimer();
  }

  server: WebSocket;
  timeout?: number;
  timeoutTimer?: NodeJS.Timeout;
}

const client = new PublishClient('channel-test');
let counter = 1;
setInterval(() => {
  client.publish("test data: " + counter);
  counter += 1;
}, 1000);
