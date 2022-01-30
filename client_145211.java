import javax.swing.*;

import java.awt.Font;
import java.awt.event.*;   
import java.net.*;
import java.util.ArrayList;
import java.io.*;

public class App extends JFrame implements ActionListener {          
    String player, opponent;
    Socket clientSocket;
    PrintWriter out;
    BufferedReader in;
    String ip;
    int port;
    ArrayList<JButton> buttons;
    JTextField textf;
    String winner = "n";
    boolean firstMM = false;
    
    public static void main(String[] args) throws IOException {  
        App app = new App(args[0], Integer.parseInt(args[1]));
    }  

    // making connection, creating a window
    App(String aip, int aport) throws IOException {
        // making connection
        ip = aip;
        port = aport; 
        clientSocket = new Socket(ip, port);
        out = new PrintWriter(clientSocket.getOutputStream(), true);
        in = new BufferedReader(new InputStreamReader(clientSocket.getInputStream()));
        
        // main window
        JFrame f = new JFrame("Tic Tac Toe");
        f.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        f.setLocation(50, 50);

        textf = new JTextField();  
        textf.setBounds(20, 20, 150, 20); 
        
        // receiving "x" or "o"
        player = in.readLine();
        if (player.equals("x")){
            opponent = "o";
            textf.setText("You go first with x");
        }
        else {
            opponent = "x";
            textf.setText("You go second with o");
        }

        // adding fields/buttons
        buttons = new ArrayList<JButton>();
        for (int i = 0; i < 9; i++) {
            JButton b = new JButton();  
            b.setBounds(20 + 50 * (i%3), 60 + 50 * (int)(Math.floor(i/3)), 50, 50); 
            b.setActionCommand("" + i);
            b.setFont(new Font("Arial", Font.PLAIN, 25));
            b.addActionListener(this);  
            f.add(b);
            buttons.add(b);
        }

        // creating the window
        f.add(textf);  
        f.setSize(200, 260);  
        f.setLayout(null);  
        f.setVisible(true);  

        // making "o" go second
        if (player.equals("o")) {
            String inp = in.readLine();
            buttons.get(Integer.valueOf(inp)).setText("x");
            firstMM = true;
            inp = in.readLine();
        }
    }

    // checking whether a field is already marked
    public Boolean occupied(int field){
        if (buttons.get(field).getText().equals("")){
            return false;
        }
        return true;
    }

    // receiving input from buttons and info from server
	public void actionPerformed(ActionEvent e) {
        try {
            // get pressed button number
            // 0 | 1 | 2
            // 3 | 4 | 5
            // 6 | 7 | 8
            String s = e.getActionCommand();
            String inp; 
            int num = Integer.valueOf(s);

            // if pressed field is empty, game isn't already settled and it's players turn
            if (!occupied(num) && winner.equals("n") && (firstMM || player.equals("x"))) {
                // send pressed button's number
                out.println(s);
                out.flush();
                // change field's label to player's
                buttons.get(num).setText(player);
        
                // await response
                do {
                    inp = in.readLine();
                    winner = in.readLine();
                } while (inp.equals(null));

                // change the label of received field to opponent's symbol
                num = Integer.valueOf(inp);
                if (num < 9) {
                    buttons.get(num).setText(opponent);
                }

                // receive the current state of the game
                if (winner.equals(player)) {
                    textf.setText("You win!");
                }  
                else if (winner.equals(opponent)) {
                    textf.setText("You lose!");
                    out.println("9");
                    out.flush();
                }
                else if (winner.equals("d")) {
                    textf.setText("Draw!");
                    out.println("9");
                    out.flush();
                }
            }
        }
        catch(IOException ex) {
            System.out.println("IOException");
        }
    }
}  