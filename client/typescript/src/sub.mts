import WebSocket from 'ws';

const url = 'ws://localhost:8080/';

class SubscriberClient {
  constructor(channel: string) {
    this.server = new WebSocket(`${url}${channel}/sub`, { timeout: 500, });
    this.server.onmessage = this.onMessage.bind(this);
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
    console.log('Subscribe: ', e.data);
  }

  private setupTimeoutTimer() {
    console.log("Timeout Reset");
    if (this.timeoutTimer) {
      clearTimeout(this.timeoutTimer);
    }
    this.timeoutTimer = setTimeout(() => { console.log("Timeout"); this.server.close() }, this.timeout);
  }

  server: WebSocket;
  timeout?: number;
  timeoutTimer?: NodeJS.Timeout;
}

const client = new SubscriberClient('channel-test');

