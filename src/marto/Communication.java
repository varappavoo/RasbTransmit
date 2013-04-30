package marto;

import java.io.DataOutputStream;
import java.io.IOException;
import java.net.Socket;
import java.net.UnknownHostException;

public class Communication {
	private final static int DEFAULT_PORT = 5234;
	public final DataOutputStream outToServer;
	private final Socket clientSocket;

	public Communication(String IP_raw, int samp_rate, float freqMHz) throws UnknownHostException, IOException {
		final String[] IP = IP_raw.trim().split(":");

		int port = DEFAULT_PORT;
		try { port = Integer.parseInt(IP[1]); } catch (Throwable t) {};
		
		clientSocket = new Socket(IP[0], port);
		outToServer = new DataOutputStream(clientSocket.getOutputStream());

		// header
		sendIntArray(
				samp_rate, 
				(int) (freqMHz * 1000000)
				);
		
	}
	
	public void sendIntArray(int ... things) throws IOException {
		int size = things.length;
		
		int checksum = 123;		
		for (int i = 0; i < size; i++) {
			outToServer.writeInt(things[i]);
			checksum ^= things[i];
		}
			
		outToServer.writeInt(checksum);
		outToServer.flush();
	}
	
	public void sendByteSamplesSlow(final int[] sound) throws IOException {
		final byte[] int_sound = new byte[sound.length];
		for (int i = 0; i < int_sound.length; i++) int_sound[i] = (byte) sound[i];
		outToServer.write(int_sound);
	}
	
	public void close() throws IOException {
		clientSocket.close();
	}
	
	@Override
	protected void finalize() throws Throwable {
		clientSocket.close();
		System.out.println("Closing connection due to GC!");
		super.finalize();
	}
	
	
}
