package marto;

import javax.sound.sampled.AudioFormat;
import javax.sound.sampled.AudioSystem;
import javax.sound.sampled.DataLine;
import javax.sound.sampled.LineUnavailableException;
import javax.sound.sampled.TargetDataLine;

public class Recorder {

	private static final int AT_A_TIME = 512;
	
	private boolean running = true;
	public final CircularBuffer cb;
	
	public Recorder(final int sampleRate) throws LineUnavailableException {
		final AudioFormat format = new AudioFormat(sampleRate, 16, 1, true, false);
		cb = new CircularBuffer(sampleRate); // 1 s buffering
		DataLine.Info info = new DataLine.Info(TargetDataLine.class, format);
	    final TargetDataLine line = (TargetDataLine) AudioSystem.getLine(info);
	    line.open(format);
	    line.start();
	    
	    new Thread() {
	    	public void run() {
	    		final byte[] buffer = new byte[AT_A_TIME];
	    		while (running) {
	    			final int read = line.read(buffer, 0, AT_A_TIME);
	    			if (read > 0)
	    				cb.add(buffer, read);
	    		}
	    	};
	    }.start();
	}
	
	public void stop() {
		running = false;
	}
	
	
}
