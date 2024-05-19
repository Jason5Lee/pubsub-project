import WebSocket from 'ws';

const url = 'ws://localhost:8080/';

class PublishClient {
  constructor(public readonly channel: string) {
    this.server = new WebSocket(url, { timeout: 500, });
    this.server.onopen = this.onOpen.bind(this);
    this.server.onmessage = this.onMessage.bind(this);
  }

  private onOpen() {
    console.log('Connected to server');
    const data = Buffer.concat([Buffer.from([0]), Buffer.from(this.channel, 'utf-8')]);
    this.server.send(data);
  }

  private onMessage(e: WebSocket.MessageEvent) {
    if (this.timeout === undefined) {
      // First message, ping duration.
      this.timeout = Number((e.data as Buffer).readBigUInt64LE(0)) + 5000;
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

  public publish(data: Buffer, cb?: ((err?: Error | undefined) => void)) {
    this.server.send(data, cb);
  }

  server: WebSocket;
  timeout?: number;
  timeoutTimer?: NodeJS.Timeout;
}

const client = new PublishClient('channel-test');

let counter = 0;
setInterval(() => {
  client.publish(Buffer.from("Message " + counter++ + "\n", 'utf-8'));
}, 1000);
