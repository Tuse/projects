import java.util.ArrayList;

//A rule in the KB (split into predicates from the LHS and RHS; see class Predicate for more info
public class Rule {
	private Predicate rhs;
	private ArrayList<Predicate> lhs;
	
	public Rule(Predicate rhs, ArrayList<Predicate> lhs) {
		this.rhs = rhs;
		this.lhs = lhs;
	}
	
	public ArrayList<Predicate> getLHS() {
		return this.lhs;
	}
	
	public Predicate getRHS() {
		return this.rhs;
	}
}
