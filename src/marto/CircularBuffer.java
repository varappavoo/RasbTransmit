package marto;

public class CircularBuffer {

	private final byte[] buffer;
	private final int buffsize;
	private int pos = 0;
	private int size = 0;
	
	public CircularBuffer(final int size) {
		buffer = new byte[size];
		buffsize = size;
	}
	
	public boolean add(final byte[] in, final int len) {
		final int putpos = (pos + size) % buffsize;

		if (size + len > buffsize)
			return false;

		if (putpos + len <= buffsize)
			System.arraycopy(in, 0, buffer, putpos, len);
		else {
			final int rem = buffsize - putpos;
			final int rem2 = len - rem;

			System.arraycopy(in, 0, buffer, putpos, rem);
			System.arraycopy(in, rem, buffer, 0, rem2);
		}
		size += len;

		return true;
	}
	
	public boolean get(final byte[] out, final int len) {

		final int nsize = size - len;

		if (size < len)
			return false;

		if (pos + len <= buffsize)
			System.arraycopy(buffer, pos, out, 0, len);
		else {
			final int rem = buffsize - pos;
			final int rem2 = len - rem;

			System.arraycopy(buffer, pos, out, 0, rem);
			System.arraycopy(buffer, 0, out, rem, rem2);
		}

		pos = (pos + len) % buffsize;
		size = (nsize < 0) ? 0 : nsize;

		return true;

	}
	
}
