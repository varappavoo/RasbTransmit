package marto;

import java.nio.ByteBuffer;

public class Processor {
	
	private static final int AT_A_TIME = 512;
	
	private boolean running = true;

	public Processor(final String IP) throws Exception {
		final Communication comm = new Communication(IP, 96000, 91.3f);
		final Recorder record = new Recorder(96000);
		
		new Thread() {
			public void run() {

				try {
					
					final byte[] buffer = new byte[AT_A_TIME];

					while(running) {

						while (!record.cb.get(buffer, AT_A_TIME)) {
							Thread.sleep(10);
						}
							
						comm.outToServer.write(buffer);

					}

					comm.close();
					record.stop();

				} catch (Exception e) {
					e.printStackTrace();
				}
			};
		}.start();
	}
	
	public void stop() {
		running = false;
	}
	
}
