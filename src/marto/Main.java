package marto;

import java.awt.EventQueue;

import javax.swing.JFrame;
import javax.swing.JTextField;
import javax.swing.JButton;
import javax.swing.JLabel;

import java.awt.event.ActionListener;
import java.awt.event.ActionEvent;

public class Main {

	private final static String DEFAULT_IP = "192.168.3.106";
	
	private JFrame frame;
	private JTextField textField;
	private Processor proc;
	
	/**
	 * Launch the application.
	 */
	public static void main(String[] args) {
		
		EventQueue.invokeLater(new Runnable() {
			public void run() {
				try {
					Main window = new Main();
					window.frame.setVisible(true);
				} catch (Exception e) {
					e.printStackTrace();
				}
			}
		});
	}

	/**
	 * Create the application.
	 */
	public Main() {
		initialize();
	}

	/**
	 * Initialize the contents of the frame.
	 */
	private void initialize() {
		frame = new JFrame();
		frame.setBounds(100, 100, 618, 395);
		frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
		frame.getContentPane().setLayout(null);
		
		textField = new JTextField();
		textField.setText(DEFAULT_IP);
		textField.setBounds(152, 13, 116, 22);
		frame.getContentPane().add(textField);
		textField.setColumns(10);
		
		JButton btnConnect = new JButton("Connect");
		btnConnect.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent arg0) {
				
				new Thread() {
					public void run() {
						try {
							
							proc = new Processor(textField.getText());
							
						} catch (Exception e) {
							error(e);
						}
					};
				}.start();
			}
		});
		btnConnect.setBounds(12, 12, 97, 25);
		frame.getContentPane().add(btnConnect);
		
		JLabel lblIpAndPort = new JLabel("IP");
		lblIpAndPort.setBounds(121, 16, 19, 16);
		frame.getContentPane().add(lblIpAndPort);
	}
	
	private void error(Throwable e) {
		e.printStackTrace();
	}
}
